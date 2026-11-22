/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_MONITOR_H__
#define __AW87XXX_MONITOR_H__

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/types.h>

#define AW_MONITOR_BIN_PARSE_VER	"V0.1.0"
#define AW_MONITOR_DEFAULT_TIME_MS	3000
#define AW_MONITOR_VMAX_NONE		0xFFFFFFFF
#define AW_MONITOR_ESD_ENABLE		1
#define AW_MONITOR_ESD_DISABLE		0

#define AW_MONITOR_BIN_HDR_VER		0x01000000
#define AW_MONITOR_BIN_DATA_TYPE	0x00020000
#define AW_MONITOR_BIN_DATA_VER_V1	1
#define AW_MONITOR_BIN_DATA_SIZE	4
#define AW_MONITOR_BIN_DATA_BYTE_LEN	4
#define AW_MONITOR_BIN_HDR_LEN		60

enum aw_monitor_bin_status {
	AW_MONITOR_CFG_WAIT = 0,
	AW_MONITOR_CFG_OK = 1,
};

enum aw_monitor_first_entry {
	AW_FIRST_ENTRY = 0,
	AW_NOT_FIRST_ENTRY = 1,
};

struct vmax_step_config {
	u32 vbat_min;
	u32 vbat_max;
	int vmax_vol;
};

struct aw_monitor_header {
	u32 monitor_switch;
	u32 monitor_time;
	u32 monitor_count;
	u32 step_count;
	u32 reserve[4];
} __packed;

struct aw_monitor_cfg {
	u32 bin_status;
	struct aw_monitor_header header;
	struct vmax_step_config *vmax_cfg;
};

struct aw_monitor {
	u8 open_dsp_en;
	u8 esd_enable;
	int dev_index;
	u8 first_entry;
	u8 timer_cnt;
	u32 vbat_sum;
	u32 custom_capacity;
	u32 pre_vmax;
	struct aw_monitor_cfg monitor_cfg;
	struct delayed_work with_dsp_work;
};

void aw_monitor_cfg_free(struct aw_monitor *monitor);
int aw_monitor_bin_parse(struct device *dev, const char *monitor_data,
			 u32 data_len);
void aw_monitor_stop(struct aw_monitor *monitor);
void aw_monitor_start(struct aw_monitor *monitor);
int aw_monitor_no_dsp_get_vmax(struct aw_monitor *monitor, int *vmax);
void aw_monitor_init(struct device *dev, struct aw_monitor *monitor,
		     struct device_node *dev_node);
void aw_monitor_exit(struct aw_monitor *monitor);

#endif /* __AW87XXX_MONITOR_H__ */
