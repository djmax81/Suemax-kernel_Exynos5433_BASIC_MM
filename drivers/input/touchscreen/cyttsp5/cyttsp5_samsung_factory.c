/*
 * cyttsp5_samsung_factory.c
 * Cypress TrueTouch(TM) Standard Product V5 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/slab.h>
#include <linux/err.h>

#include "cyttsp5_regs.h"


/************************************************************************
 * Macros, Structures
 ************************************************************************/
#define MAX_NODE_NUM 900 /* 30 * 30 */
#define MAX_INPUT_HEADER_SIZE 12
#define MAX_GIDAC_NODES 32
#define MAX_LIDAC_NODES (MAX_GIDAC_NODES * 30)

struct class *sec_class;
extern struct class *sec_class;

enum {
	FACTORYCMD_WAITING,
	FACTORYCMD_RUNNING,
	FACTORYCMD_OK,
	FACTORYCMD_FAIL,
	FACTORYCMD_NOT_APPLICABLE
};

enum {
	IDAC_GLOBAL,
	IDAC_LOCAL,
};
#define FACTORY_CMD(name, func) .cmd_name = name, .cmd_func = func

struct factory_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func)(void *device_data);
};

/************************************************************************
 * function def
 ************************************************************************/
static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_raw_count(void *device_data);
static void get_difference(void *device_data);
static void get_local_idac(void *device_data);
static void get_global_idac(void *device_data);
static void run_raw_count_read(void *device_data);
static void run_difference_read(void *device_data);
static void run_local_idac_read(void *device_data);
static void run_global_idac_read(void *device_data);
static void not_support_cmd(void *device_data);

/************************************************************************
 * cmd table
 ************************************************************************/
struct factory_cmd factory_cmds[] = {
	{FACTORY_CMD("fw_update", fw_update),},
	{FACTORY_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{FACTORY_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{FACTORY_CMD("get_config_ver", get_config_ver),},
	{FACTORY_CMD("get_threshold", get_threshold),},
	{FACTORY_CMD("module_off_master", module_off_master),},
	{FACTORY_CMD("module_on_master", module_on_master),},
	{FACTORY_CMD("module_off_slave", not_support_cmd),},
	{FACTORY_CMD("module_on_slave", not_support_cmd),},
	{FACTORY_CMD("get_chip_vendor", get_chip_vendor),},
	{FACTORY_CMD("get_chip_name", get_chip_name),},
	{FACTORY_CMD("get_x_num", get_x_num),},
	{FACTORY_CMD("get_y_num", get_y_num),},
	{FACTORY_CMD("get_raw_count", get_raw_count),},
	{FACTORY_CMD("get_difference", get_difference),},
	{FACTORY_CMD("get_local_idac", get_local_idac),},
	{FACTORY_CMD("get_global_idac", get_global_idac),},
	{FACTORY_CMD("run_raw_count_read", run_raw_count_read),},
	{FACTORY_CMD("run_difference_read", run_difference_read),},
	{FACTORY_CMD("run_local_idac_read", run_local_idac_read),},
	{FACTORY_CMD("run_global_idac_read", run_global_idac_read),},
	{FACTORY_CMD("not_support_cmd", not_support_cmd),},
};

/************************************************************************
 * helpers
 ************************************************************************/
static void set_cmd_result(struct cyttsp5_samsung_factory_data* sfd,
		char *strbuff, int len)
{
	strncat(sfd->factory_cmd_result, strbuff, len);
}

static void set_default_result(struct cyttsp5_samsung_factory_data* sfd)
{
	char delim = ':';

	memset(sfd->factory_cmd_result, 0x00, ARRAY_SIZE(sfd->factory_cmd_result));
	memcpy(sfd->factory_cmd_result, sfd->factory_cmd, strlen(sfd->factory_cmd));
	strncat(sfd->factory_cmd_result, &delim, 1);
}

/************************************************************************
 * commands
 ************************************************************************/
static void fw_update(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	cyttsp5_upgrade_firmware_from_platform upgrade_firmware_from_platform;
	char strbuff[16] = {0};
	int rc = 0;

	set_default_result(sfd);

	upgrade_firmware_from_platform = _cyttsp5_request_upgrade_firmware_from_platform(sfd->dev);
	if (upgrade_firmware_from_platform)
		rc = upgrade_firmware_from_platform(sfd->dev, sfd->factory_cmd_param[0] == 1);
	else
		rc = -1;

	if(rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%s", "OK");
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		dev_err(sfd->dev, "%s: rc=%d\n", __func__, rc);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_platform_data *pdata = dev_get_platdata(sfd->dev);
	struct cyttsp5_touch_firmware *fw = pdata->loader_pdata->fw;
		char strbuff[16] = {0};

	set_default_result(sfd);

	if (fw) {
		snprintf(strbuff, sizeof(strbuff), "CY%02x%04x",
			fw->hw_version, fw->fw_version);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "CY%02x%02x%02x",
		sti->hw_version, sti->fw_versionh, sti->fw_versionl);
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_config_ver(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};

	set_default_result(sfd);
#ifdef SAMSUNG_TSP_INFO
	snprintf(strbuff, sizeof(strbuff), "%u", sti->config_version);

	sfd->factory_cmd_state = FACTORYCMD_OK;
#else
	sprintf(strbuff, "%s", "NG");

	sfd->factory_cmd_state = FACTORYCMD_FAIL;
#endif
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_threshold(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};
	int rc = 0;
#if 0
	u8 parm_id = 0x1a;
	u32 test;
#endif
	set_default_result(sfd);

	snprintf(strbuff, sizeof(strbuff), "%u", get_unaligned_be16(&sti->thresholdh));
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	if (rc < 0)
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	else
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void module_off_master(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[3] = {0};

	set_default_result(sfd);
#if 0
	_cyttsp5_unsubscribe_attention(sfd->dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	_cyttsp5_unsubscribe_attention(sfd->dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp5_startup_attention, 0);
#endif
	cyttsp5_core_suspend(sfd->dev);

	snprintf(strbuff, sizeof(strbuff), "%s", "OK");
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void module_on_master(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[3] = {0};

	set_default_result(sfd);
#if 0
	_cyttsp5_subscribe_attention(sfd->dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	_cyttsp5_subscribe_attention(sfd->dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp5_startup_attention, 0);

	_cyttsp5_subscribe_attention(sfd->dev, CY_ATTEN_WAKE, CY_MODULE_MT,
		cyttsp5_mt_wake_attention, 0);
#endif
	cyttsp5_core_resume(sfd->dev);

	snprintf(strbuff, sizeof(strbuff), "%s", "OK");
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_chip_vendor(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "%s", "Cypress");
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_chip_name(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "%s", "CYTMA545");
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_x_num(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);

	sprintf(strbuff, "%u", sfd->si->sensing_conf_data.electrodes_x);
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void get_y_num(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);

	sprintf(strbuff, "%u", sfd->si->sensing_conf_data.electrodes_y);

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static inline s16 node_value_s16(u8* buf, u16 node)
{
	return (s16)get_unaligned_le16(buf + node * 2);
}

static void get_raw_diff(struct cyttsp5_samsung_factory_data* sfd,
	struct cyttsp5_sfd_panel_scan_data *panel_scan_data)
{
	char strbuff[16] = {0};
	s16 value = 0;

	set_default_result(sfd);

	if ((sfd->factory_cmd_param[0] < 0) ||
		(sfd->factory_cmd_param[0] >= sfd->num_all_nodes)) {
		dev_err(sfd->dev, "%s: parameter %d is wrong\n",
					__func__, sfd->factory_cmd_param[0]);

		sprintf(strbuff, "%s", "NG");
		set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		if (panel_scan_data->element_size == 2)
			value = node_value_s16(panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
				sfd->factory_cmd_param[0]);
		else
			value = panel_scan_data->buf[CY_CMD_RET_PANEL_HDR + (u8)sfd->factory_cmd_param[0]];

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node %d = %d\n",
					__func__, sfd->factory_cmd_param[0], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));

}

static void get_raw_count(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_raw_diff(sfd, &sfd->raw);
}

static void get_difference(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_raw_diff(sfd, &sfd->diff);
}

static void find_max_min_s16(u8* buf, int num_nodes, s16 *max_value, s16 *min_value)
{
	int i;
	*max_value = 0x8000;
	*min_value = 0x7FFF;

	for (i = 0 ; i < num_nodes ; i++) {
		*max_value = max((s16)*max_value, (s16)get_unaligned_le16(buf));
		*min_value = min((s16)*min_value, (s16)get_unaligned_le16(buf));
		buf += 2;
	}
}
static void find_max_min_s8(u8* buf, int num_nodes, s16 *max_value, s16 *min_value)
{
	int i;
	*max_value = 0x8000;
	*min_value = 0x7FFF;

	for (i = 0 ; i < num_nodes ; i++) {
		*max_value = max((s8)*max_value, (s8)(*buf));
		*min_value = min((s8)*min_value, (s8)(*buf));
		buf += 1;
	}
}

static int retrieve_panel_scan(struct cyttsp5_samsung_factory_data* sfd,
		u8* buf, u8 data_id, int num_nodes, u8* r_element_size/*in bytes*/)
{
	int rc = 0;
	int elem = num_nodes;
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;
	u8 element_size = 0;

	/* fill buf with header and data */
	rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0, elem_offset, elem,
		data_id, buf, &config, &actual_read_len, NULL);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(buf);
	buf_offset = buf + length;

	element_size = config & CY_CMD_RET_PANEL_ELMNT_SZ_MASK;
	*r_element_size = element_size;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		/* append data to the buf */
		rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0, elem_offset, elem,
				data_id, NULL, &config, &actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len * element_size;
		buf_offset = buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

static int panel_scan_and_retrieve(struct cyttsp5_samsung_factory_data* sfd,
		u8 data_id, struct cyttsp5_sfd_panel_scan_data *panel_scan_data)
{
	struct device *dev = sfd->dev;
	int rc = 0;

	dev_dbg(sfd->dev, "%s, line=%d\n", __func__, __LINE__ );

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = sfd->corecmd->cmd->exec_panel_scan(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: exec panel scan failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_panel_scan(sfd, panel_scan_data->buf, data_id,
		sfd->num_all_nodes, &panel_scan_data->element_size);
	if (rc < 0) {
		dev_err(dev, "%s: retrieve_panel_scan raw count failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	if (panel_scan_data->element_size == 2)
		find_max_min_s16(panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
			sfd->num_all_nodes, &panel_scan_data->max, &panel_scan_data->min);
	else
		find_max_min_s8(panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
			sfd->num_all_nodes, &panel_scan_data->max, &panel_scan_data->min);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
	}

release_exclusive:
	rc = cyttsp5_release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

static void run_raw_count_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = panel_scan_and_retrieve(sfd, CY_MUT_RAW, &sfd->raw);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%d,%d", sfd->raw.min, sfd->raw.max);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		sprintf(strbuff, "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

#if 0
{
	int i;
	dev_info(sfd->dev, "%s: raw : \n", __func__);
	for (i = 0; i < (sfd->num_all_nodes * 2); i++)
	{
		dev_info(sfd->dev, "%s: 0x%02x\n", __func__, sfd->raw.buf[CY_CMD_RET_PANEL_HDR + i]);
	}
}
#endif

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void run_difference_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = panel_scan_and_retrieve(sfd, CY_MUT_DIFF, &sfd->diff);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%d,%d", sfd->diff.min, sfd->diff.max);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		sprintf(strbuff, "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

/************************************************************************
 * commands - IDAC
 ************************************************************************/
static u16 gidac_node_num(struct cyttsp5_samsung_factory_data* sfd)
{
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	return sti->gidac_nodes;
}
static u16 lidac_node_num(struct cyttsp5_samsung_factory_data* sfd)
{
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	return sti->gidac_nodes * sti->rx_nodes;
}

static void find_max_min_u8(u8* buf, int num_nodes, u8 *max_value, u8 *min_value)
{
	int i;
	*max_value = 0x00;
	*min_value = 0xff;

	for (i = 0 ; i < num_nodes ; i++) {
		*max_value = max((u8)*max_value, (u8)(*buf));
		*min_value = min((u8)*min_value, (u8)(*buf));
		buf += 1;
	}
}

static int retrieve_data_structure(struct cyttsp5_samsung_factory_data* sfd,
		u8 data_id, u8* buf, int num_nodes)
{
	int rc = 0;
	int elem = num_nodes;
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;

	/* fill buf with header and data */
	rc = sfd->corecmd->cmd->retrieve_data_structure(sfd->dev, 0, elem_offset, elem,
		data_id, buf, &config, &actual_read_len, NULL);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(buf);
	buf_offset = buf + length;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		/* append data to the buf */
		rc = sfd->corecmd->cmd->retrieve_data_structure(sfd->dev, 0, elem_offset, elem,
				data_id, NULL, &config, &actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len;
		buf_offset = buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

static void get_idac(struct cyttsp5_samsung_factory_data* sfd,
	int max_node_num, u8 *buf)
{
	char strbuff[16] = {0};
	u8 value = 0;

	set_default_result(sfd);

	if ((sfd->factory_cmd_param[0] < 0) ||
		(sfd->factory_cmd_param[0] >= max_node_num)) {
		dev_err(sfd->dev, "%s: parameter %d is wrong\n",
					__func__, sfd->factory_cmd_param[0]);

		sprintf(strbuff, "%s", "NG");
		set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		value = buf[(u8)sfd->factory_cmd_param[0]];

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node %d = %d\n",
					__func__, sfd->factory_cmd_param[0], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));

}

static void get_global_idac(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_idac(sfd, gidac_node_num(sfd), sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR);
}

static void get_local_idac(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_idac(sfd, lidac_node_num(sfd), sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR + gidac_node_num(sfd));
}

static int retrieve_mutual_idac(struct cyttsp5_samsung_factory_data* sfd,
		u8 type)
{
	struct device *dev = sfd->dev;
	int rc = 0;

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_data_structure(sfd, CY_PWC_MUT, sfd->mutual_idac.buf,
			gidac_node_num(sfd)+lidac_node_num(sfd));
	if (rc < 0) {
		dev_err(dev, "%s: retrieve_data_structure failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}
	if (type == IDAC_GLOBAL)
		find_max_min_u8(sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR,
			gidac_node_num(sfd), &sfd->mutual_idac.gidac_max, &sfd->mutual_idac.gidac_min);
	else
		find_max_min_u8(sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR + gidac_node_num(sfd),
			lidac_node_num(sfd), &sfd->mutual_idac.lidac_max, &sfd->mutual_idac.lidac_min);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

release_exclusive:
	rc = cyttsp5_release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

static void run_idac_read(struct cyttsp5_samsung_factory_data* sfd,
	u8 type)
{
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = retrieve_mutual_idac(sfd, type);
	if (rc == 0) {
		if (type == IDAC_GLOBAL)
			snprintf(strbuff, sizeof(strbuff), "%d,%d",
				sfd->mutual_idac.gidac_min, sfd->mutual_idac.gidac_max);
		else
			snprintf(strbuff, sizeof(strbuff), "%d,%d",
				sfd->mutual_idac.lidac_min, sfd->mutual_idac.lidac_max);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		sprintf(strbuff, "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
}

static void run_global_idac_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	run_idac_read(sfd, IDAC_GLOBAL);
}

static void run_local_idac_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	run_idac_read(sfd, IDAC_LOCAL);
}

static void not_support_cmd(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	sprintf(strbuff, "%s", "NA");
	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_NOT_APPLICABLE;
	dev_info(sfd->dev, "%s: \"%s(%d)\"\n", __func__,
		strbuff, strnlen(strbuff, sizeof(strbuff)));
	return;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct factory_cmd *factory_cmd_ptr = NULL;
	int param_cnt = 0;
	int ret, len, i;
	char *cur, *start, *end;
	char strbuff[FACTORY_CMD_STR_LEN] = {0};
	char delim = ',';
	bool cmd_found = false;

	if (sfd->factory_cmd_is_running == true) {
		dev_err(sfd->dev, "factory_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = true;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_RUNNING;

	for (i = 0; i < ARRAY_SIZE(sfd->factory_cmd_param); i++)
		sfd->factory_cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(sfd->factory_cmd, 0x00, ARRAY_SIZE(sfd->factory_cmd));
	memcpy(sfd->factory_cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(strbuff, buf, cur - buf);
	else
		memcpy(strbuff, buf, len);

	/* find command */
	list_for_each_entry(factory_cmd_ptr,
			&sfd->factory_cmd_list_head, list) {
		if (!strcmp(strbuff, factory_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(factory_cmd_ptr,
				&sfd->factory_cmd_list_head, list) {
			if (!strcmp("not_support_cmd", factory_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(strbuff, 0x00, ARRAY_SIZE(strbuff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(strbuff, start, end - start);
				*(strbuff + strlen(strbuff)) = '\0';
				ret = kstrtoint(strbuff, 10,\
						sfd->factory_cmd_param + param_cnt);
				start = cur + 1;
				memset(strbuff, 0x00, ARRAY_SIZE(strbuff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(sfd->dev, "cmd = %s\n", factory_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(sfd->dev, "cmd param %d= %d\n", i,
			sfd->factory_cmd_param[i]);

	factory_cmd_ptr->cmd_func(sfd);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	char strbuff[16] = {0};

	dev_info(sfd->dev, "tsp cmd: status:%d, PAGE_SIZE=%ld\n",
			sfd->factory_cmd_state, PAGE_SIZE);

	if (sfd->factory_cmd_state == FACTORYCMD_WAITING)
		snprintf(strbuff, sizeof(strbuff), "WAITING");

	else if (sfd->factory_cmd_state == FACTORYCMD_RUNNING)
		snprintf(strbuff, sizeof(strbuff), "RUNNING");

	else if (sfd->factory_cmd_state == FACTORYCMD_OK)
		snprintf(strbuff, sizeof(strbuff), "OK");

	else if (sfd->factory_cmd_state == FACTORYCMD_FAIL)
		snprintf(strbuff, sizeof(strbuff), "FAIL");

	else if (sfd->factory_cmd_state == FACTORYCMD_NOT_APPLICABLE)
		snprintf(strbuff, sizeof(strbuff), "NOT_APPLICABLE");

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);

	dev_info(sfd->dev, "tsp cmd: result: %s\n", sfd->factory_cmd_result);

	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_WAITING;

	return snprintf(buf, PAGE_SIZE, "%s\n", sfd->factory_cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_touch_factory_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

/************************************************************************
 * init
 ************************************************************************/
int cyttsp5_samsung_factory_probe(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;
	int rc = 0;
	int i;

	sfd->dev = dev;

	sfd->corecmd = cyttsp5_get_commands();
	if (!sfd->corecmd) {
		dev_err(dev, "%s: core cmd not available\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	sfd->si = _cyttsp5_request_sysinfo(dev);
	if (!sfd->si) {
		dev_err(dev, "%s: Fail get sysinfo pointer from core\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	dev_dbg(dev, "%s: electrodes_x=%d\n", __func__,
		sfd->si->sensing_conf_data.electrodes_x);
	dev_dbg(dev, "%s: electrodes_y=%d\n", __func__,
		sfd->si->sensing_conf_data.electrodes_y);

	sfd->num_all_nodes = sfd->si->sensing_conf_data.electrodes_x *
		sfd->si->sensing_conf_data.electrodes_y;
	if (sfd->num_all_nodes > MAX_NODE_NUM) {
		dev_err(dev, "%s: sensor node num(%d) exceeds limits\n", __func__,
			sfd->num_all_nodes);
		rc = -EINVAL;
		goto error_return;
	}

	sfd->raw.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->raw.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->raw.buf\n", __func__);
		rc = -ENOMEM;
		goto error_return;
	}

	sfd->diff.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->diff.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->diff.buf\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_difference_buf;
	}

	sfd->mutual_idac.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		gidac_node_num(sfd) + lidac_node_num(sfd)), GFP_KERNEL);
	if (sfd->mutual_idac.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->mutual_idac.buf\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_idac_buf;
	}

	INIT_LIST_HEAD(&sfd->factory_cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(factory_cmds); i++)
		list_add_tail(&factory_cmds[i].list, &sfd->factory_cmd_list_head);

	mutex_init(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;

	sfd->factory_dev = device_create(sec_class, NULL, 0, sfd, "tsp");
	if (IS_ERR(sfd->factory_dev)) {
		dev_err(sfd->dev, "Failed to create device for the sysfs\n");
		goto error_device_create;
	}

	rc = sysfs_create_group(&sfd->factory_dev->kobj,
		&sec_touch_factory_attr_group);
	if (rc) {
		dev_err(sfd->dev, "Failed to create sysfs group\n");
		goto error_sysfs_create_group;
	}

	sfd->sysfs_nodes_created = true;

	dev_dbg(sfd->dev, "%s success. rc=%d\n", __func__, rc);
	return 0;

error_sysfs_create_group:
error_device_create:
	kfree(sfd->mutual_idac.buf);
error_alloc_idac_buf:
	kfree(sfd->diff.buf);
error_alloc_difference_buf:
	kfree(sfd->raw.buf);
error_return:
	dev_err(dev, "%s failed. rc=%d\n", __func__, rc);
	return rc;
}

int cyttsp5_samsung_factory_release(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;

	if (sfd->sysfs_nodes_created) {
		sysfs_remove_group(&sfd->factory_dev->kobj,
			&sec_touch_factory_attr_group);
		kfree(sfd->mutual_idac.buf);
		kfree(sfd->diff.buf);
		kfree(sfd->raw.buf);
		sfd->sysfs_nodes_created = false;
	}
	dev_dbg(dev, "%s\n",__func__);

	return 0;
}

