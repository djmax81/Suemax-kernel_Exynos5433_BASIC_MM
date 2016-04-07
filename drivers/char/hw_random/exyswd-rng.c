/*
 * exyswd-rng.c - Random Number Generator driver for the exynos
 *
 * Copyright (C) 2013 Samsung Electronics
 * Sehee Kim <sehi.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <mach/smc.h>

#define HWRNG_RET_OK			0
#define HWRNG_RET_INVALID_ERROR		1
#define HWRNG_RET_RETRY_ERROR		2
#define HWRNG_RET_INVALID_FLAG_ERROR	3
#define HWRNG_RET_TEST_ERROR		4

#define EXYRNG_MAX_FAILURES		25
#define EXYRNG_START_UP_SIZE		4096
#define EXYRNG_RETRY_MAX_COUNT		1000

uint32_t hwrng_read_flag;
static struct hwrng rng;

spinlock_t hwrandom_lock;
static size_t test_failures;
#if defined(CONFIG_EXYRNG_FAIL_POLICY_DISABLE)
static int hwrng_disabled;
#endif
static int start_up_test;

#ifdef CONFIG_EXYRNG_DEBUG
#define exyrng_debug(args...)	printk(KERN_INFO args)
#else
#define exyrng_debug(args...)
#endif

void exynos_swd_test_fail(void)
{
#if defined(CONFIG_EXYRNG_FAIL_POLICY_DISABLE)
	hwrng_disabled = 1;
	printk("[ExyRNG] disabled for test failures\n");
#else /* defined(CONFIG_EXYRNG_POLICY_RESET) */
	panic("[ExyRNG] It failed to health tests. It means that it detects "
	"the malfunction of TRNG(HW) which generates random numbers. If it "
	"doesn't offer enough entropy, it should not be used. The system "
	"reset could be a way to solve it. The health tests are designed "
	"to have the false positive rate of approximately once per billion "
	"based on min-entropy of TRNG.\n");
#endif
}

static int exynos_swd_startup_test(void)
{
	uint32_t start_up_size;
	uint32_t retry_cnt;
	int ret = 0;

	start_up_size = EXYRNG_START_UP_SIZE;
	retry_cnt = 0;

	while (start_up_size) {
		ret = exynos_smc(SMC_CMD_RANDOM, HWRNG_GET_DATA, 1, 0);
		if (ret == HWRNG_RET_RETRY_ERROR) {
			if (retry_cnt++ > EXYRNG_RETRY_MAX_COUNT) {
				printk("[ExyRNG] exceed retry in test\n");
				return -EFAULT;
			}
			usleep_range(50, 100);
			continue;
		}

		if (ret == HWRNG_RET_TEST_ERROR) {
			exynos_swd_test_fail();
			return -EFAULT;
		}

		if (ret != HWRNG_RET_OK) {
			return -EFAULT;
			exyrng_debug("[ExyRNG] failed to get random\n");
		}

		start_up_size -= 32;
		retry_cnt = 0;
	}

	return 0;
}

static int exynos_swd_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	uint32_t *read_buf = data;
	uint32_t read_size = max;
	uint32_t r_data[2];
	unsigned long flag;
	uint32_t retry_cnt;
	int32_t ret;

	register u32 reg0 __asm__("r0");
	register u32 reg1 __asm__("r1");
	register u32 reg2 __asm__("r2");
	register u32 reg3 __asm__("r3");

#if defined(CONFIG_EXYRNG_FAIL_POLICY_DISABLE)
	if (hwrng_disabled)
		return -EPERM;
#endif

	reg0 = 0;
	reg1 = 0;
	reg2 = 0;
	reg3 = 0;

	spin_lock_irqsave(&hwrandom_lock, flag);
	ret = exynos_smc(SMC_CMD_RANDOM, HWRNG_INIT, 0, 0);
	if (ret != HWRNG_RET_OK) {
		spin_unlock_irqrestore(&hwrandom_lock, flag);
		msleep(1);
		return -EFAULT;
	}
	hwrng_read_flag = 1;
	spin_unlock_irqrestore(&hwrandom_lock, flag);

	if (start_up_test) {
		ret = exynos_swd_startup_test();
		if (ret != HWRNG_RET_OK)
			goto out;

		start_up_test = 0;
		exyrng_debug("[ExyRNG] passed the start-up test\n");
	}

	retry_cnt = 0;
	while (read_size) {
		spin_lock_irqsave(&hwrandom_lock, flag);
		ret = exynos_smc(SMC_CMD_RANDOM, HWRNG_GET_DATA, 0, 0);
		__asm__ volatile(
			"\t"
			: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
		);
		r_data[0] = reg2;
		r_data[1] = reg3;
		spin_unlock_irqrestore(&hwrandom_lock, flag);

		if (ret == HWRNG_RET_RETRY_ERROR) {
			if (retry_cnt++ > EXYRNG_RETRY_MAX_COUNT) {
				printk("[ExyRNG] exceed retry in read\n");
				ret = -EFAULT;
				goto out;
			}
			usleep_range(50, 100);
			continue;
		}

		if (ret == HWRNG_RET_TEST_ERROR) {
			if (++test_failures > EXYRNG_MAX_FAILURES) {
#if defined(CONFIG_EXYRNG_FIPS_COMPLIANCE)
				exynos_swd_test_fail();
#else
				test_failures = 0;
#endif
			}
			exyrng_debug("[ExyRNG] failed to continuous test\n");
			ret = -EAGAIN;
			goto out;
		}

		if (ret != HWRNG_RET_OK) {
			ret = -EFAULT;
			goto out;
		}

		*(uint32_t*)(read_buf++) = r_data[0];
		*(uint32_t*)(read_buf++) = r_data[1];

		read_size -= 8;
		retry_cnt = 0;
	}

	ret = max;

out:
	spin_lock_irqsave(&hwrandom_lock, flag);
	hwrng_read_flag = 0;
	exynos_smc(SMC_CMD_RANDOM, HWRNG_EXIT, 0, 0);
	spin_unlock_irqrestore(&hwrandom_lock, flag);

	return ret;
}

static int exyswd_rng_probe(struct platform_device *pdev)
{
	rng.name = "exyswd_rng";
	rng.read = exynos_swd_read;

	spin_lock_init(&hwrandom_lock);
#if defined(CONFIG_EXYRNG_FIPS_COMPLIANCE)
	start_up_test = 1;
#endif

	return hwrng_register(&rng);
}

static int exyswd_rng_remove(struct platform_device *pdev)
{
	hwrng_unregister(&rng);

	return 0;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int exyswd_rng_suspend(struct device *dev)
{
	unsigned long flag;

	spin_lock_irqsave(&hwrandom_lock, flag);
	if (hwrng_read_flag)
		exynos_smc(SMC_CMD_RANDOM, HWRNG_EXIT, 0, 0);
	spin_unlock_irqrestore(&hwrandom_lock, flag);

	return 0;
}

static int exyswd_rng_resume(struct device *dev)
{
	unsigned long flag;

	spin_lock_irqsave(&hwrandom_lock, flag);
#if defined(CONFIG_EXYRNG_FIPS_COMPLIANCE)
	exynos_smc(SMC_CMD_RANDOM, HWRNG_RESUME, 0, 0);
#endif
	if (hwrng_read_flag)
		exynos_smc(SMC_CMD_RANDOM, HWRNG_INIT, 0, 0);
	spin_unlock_irqrestore(&hwrandom_lock, flag);

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(exyswd_rng_pm_ops, exyswd_rng_suspend, exyswd_rng_resume, NULL);

static struct platform_driver exyswd_rng_driver = {
	.probe		= exyswd_rng_probe,
	.remove		= exyswd_rng_remove,
	.driver		= {
		.name	= "exyswd_rng",
		.owner	= THIS_MODULE,
		.pm     = &exyswd_rng_pm_ops,
	},
};

static struct platform_device exyswd_rng_device = {
	.name = "exyswd_rng",
	.id = -1,
};

static int __init exyswd_rng_init(void)
{
	int ret;

	ret = platform_device_register(&exyswd_rng_device);
	if (ret)
		return ret;

	return platform_driver_register(&exyswd_rng_driver);
}

static void __exit exyswd_rng_exit(void)
{
	platform_driver_unregister(&exyswd_rng_driver);
	platform_device_unregister(&exyswd_rng_device);
}

module_init(exyswd_rng_init);
module_exit(exyswd_rng_exit);

MODULE_DESCRIPTION("EXYNOS H/W Random Number Generator driver");
MODULE_AUTHOR("Sehee Kim <sehi.kim@samsung.com>");
MODULE_LICENSE("GPL");
