/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_ACF_BIN_H__
#define __AW87XXX_ACF_BIN_H__

#include <linux/device.h>
#include <linux/types.h>
#include "aw87xxx_device.h"

#define AW_ACF_FILE_ID			0x0a15f908
#define AW_ACF_HDR_VERSION_0_0_0_1	0x00000001
#define AW_ACF_HDR_VERSION_1_0_0_0	0x01000000

#define AW_PROFILE_STR_MAX		32
#define AW_DEV_NAME_MAX			16
#define AW_PROFILE_MAX			12

enum aw_cfg_dde_type {
	AW_DEV_NONE_TYPE_ID = 0xFFFFFFFF,
	AW_DEV_TYPE_ID = 0x00000000,
	AW_SKT_TYPE_ID = 0x00000001,
	AW_DEV_DEFAULT_TYPE_ID = 0x00000002,
};

enum aw_sec_data_type {
	AW_BIN_TYPE_REG = 0x00000000,
	AW_BIN_TYPE_DSP = 0x00000001,
	AW_BIN_TYPE_DSP_CFG = 0x00000002,
	AW_BIN_TYPE_DSP_FW = 0x00000003,
	AW_BIN_TYPE_HDR_REG = 0x00000004,
	AW_BIN_TYPE_HDR_DSP_CFG = 0x00000005,
	AW_BIN_TYPE_HDR_DSP_FW = 0x00000006,
	AW_BIN_TYPE_MUTLBIN = 0x00000007,
	AW_BIN_TYPE_SKT_PROJECT = 0x00000008,
	AW_BIN_TYPE_DSP_PROJECT = 0x00000009,
	AW_BIN_TYPE_MONITOR = 0x0000000A,
	AW_BIN_TYPE_MAX,
};

enum aw_profile_status {
	AW_PROFILE_WAIT = 0,
	AW_PROFILE_OK = 1,
};

enum aw_acf_load_status {
	AW_ACF_WAIT = 0,
	AW_ACF_UPDATE = 1,
};

struct aw_acf_hdr {
	u32 file_id;
	char project[24];
	char custom[16];
	u32 hdr_version;
	u32 author_id;
	u32 ddt_size;
	u32 ddt_num;
	u32 ddt_offset;
	u32 reserve[4];
} __packed;

struct aw_dde_v_0_0_0_1 {
	u32 type;
	char dev_name[AW_DEV_NAME_MAX];
	u16 dev_index;
	s16 dev_bus;
	s16 dev_addr;
	u16 dev_profile;
	u32 data_type;
	u32 data_size;
	u32 data_offset;
	u32 data_crc;
	u32 reserve[5];
} __packed;

struct aw_dde_v_1_0_0_0 {
	u32 type;
	char dev_name[AW_DEV_NAME_MAX];
	u16 dev_index;
	s16 dev_bus;
	s16 dev_addr;
	u16 dev_profile;
	u32 data_type;
	u32 data_size;
	u32 data_offset;
	u32 data_crc;
	char dev_profile_str[AW_PROFILE_STR_MAX];
	u32 chip_id;
	u32 reserve[4];
} __packed;

struct aw_prof_desc {
	u32 prof_st;
	char *prof_name;
	char dev_name[AW_DEV_NAME_MAX];
	struct aw_data_container data_container;
};

struct aw_prof_info {
	int count;
	int status;
	int prof_type;
	char (*prof_name_list)[AW_PROFILE_STR_MAX];
	struct aw_prof_desc *prof_desc;
};

struct aw_acf_info {
	int fw_load_retry_cnt;
	int fw_size;
	u16 dev_index;
	char *fw_data;
	int product_cnt;
	const char **product_tab;
	struct aw_device *aw_dev;
	struct aw_acf_hdr acf_hdr;
	struct aw_prof_info prof_info;
};

void aw_acf_init(struct aw_device *aw_dev, struct aw_acf_info *acf_info,
		 int dev_index);
int aw_acf_parse(struct device *dev, struct aw_acf_info *acf_info);
void aw_acf_profile_free(struct device *dev, struct aw_acf_info *acf_info);
struct aw_prof_desc *aw_acf_get_prof_desc_form_name(struct device *dev,
						    struct aw_acf_info *acf_info,
						    char *profile_name);
int aw_acf_get_prof_index_form_name(struct device *dev,
				    struct aw_acf_info *acf_info,
				    char *profile_name);
char *aw_acf_get_prof_name_form_index(struct device *dev,
				      struct aw_acf_info *acf_info, int index);
int aw_acf_get_profile_count(struct device *dev, struct aw_acf_info *acf_info);
char *aw_acf_get_prof_off_name(struct device *dev,
			       struct aw_acf_info *acf_info);

#endif /* __AW87XXX_ACF_BIN_H__ */
