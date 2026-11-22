/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_H__
#define __AW87XXX_H__

#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <sound/soc.h>
#include "aw87xxx_device.h"
#include "aw87xxx_acf_bin.h"
#include "aw87xxx_monitor.h"

#define AW87XXX_DRIVER_VERSION		"v2.1.0"
#define AW87XXX_I2C_NAME		"aw87xxx_pa"
#define AW87XXX_FW_NAME_DEFAULT		"aw87xxx_acf.bin"
#define AW87XXX_FW_NAME_MAX		64
#define AW87XXX_LOAD_FW_DELAY_MS	3000
#define AW87XXX_LOAD_FW_RETRY_DELAY_MS	2000
#define AW87XXX_LOAD_FW_RETRY_MAX	3
#define AW87XXX_KCONTROL_NUM		2

enum aw87xxx_awrw_status {
	AW87XXX_AWRW_WRITE = 0,
	AW87XXX_AWRW_READ = 1,
};

struct aw_awrw_ctrl {
	u8 status;
	int reg_num;
	int reg_addr;
};

struct aw87xxx {
	char fw_name[AW87XXX_FW_NAME_MAX];
	int dev_index;
	char *current_profile;
	char prof_off_name[AW_PROFILE_STR_MAX];
	int fw_status;
	struct device *dev;
	struct mutex reg_lock;
	struct aw_device aw_dev;
	struct aw_awrw_ctrl awrw_ctrl;
	struct delayed_work fw_load_work;
	struct aw_acf_info acf_info;
	struct snd_soc_component *codec;
	struct list_head list;
	struct aw_monitor monitor;
};

char *aw87xxx_show_current_profile(int dev_index);
int aw87xxx_set_profile(int dev_index, char *profile);
int aw87xxx_add_codec_controls(void *codec);

int aw87xxx_update_profile(struct aw87xxx *aw87xxx, char *profile);
int aw87xxx_esd_update_profile(struct aw87xxx *aw87xxx, char *profile);
int aw87xxx_awrw_write(struct aw87xxx *aw87xxx, const char *buf);
int aw87xxx_dtsi_dev_index_check(struct aw87xxx *aw87xxx);

#endif /* __AW87XXX_H__ */
