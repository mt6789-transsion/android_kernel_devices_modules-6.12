/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include "aw87xxx_acf_bin.h"
#include "aw87xxx_bin_parse.h"
#include "aw87xxx_monitor.h"
#include "aw87xxx_log.h"

static char *g_profile_name[AW_PROFILE_MAX] = {
	"Music",
	"Voice",
	"Voip",
	"Ringtone",
	"Ringtone_hs",
	"Lowpower",
	"Bypass",
	"Mmi",
	"Fm",
	"Notification",
	"Receiver",
	"Off",
};

static char *g_prof_off_name_tab[] = {
	"Off",
	"OFF",
	"off",
	"oFF",
	"power_down",
};

static int aw_crc8_check(unsigned char *data, unsigned int data_size,
			 unsigned int crc_value)
{
	unsigned char crc = 0x00;
	unsigned char *p = data;
	unsigned int i = 0;

	while (data_size--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++) {
			if (crc & 0x01)
				crc = (crc >> 1) ^ 0x8c;
			else
				crc >>= 1;
		}
	}

	if (crc != (unsigned char)crc_value)
		return -EINVAL;

	return 0;
}

static int aw_check_prof_str_is_off(const char *prof_str)
{
	int i = 0;

	if (!prof_str)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(g_prof_off_name_tab); i++) {
		if (strnstr(prof_str, g_prof_off_name_tab[i], AW_PROFILE_STR_MAX))
			return 0;
	}

	return -EINVAL;
}

static void aw_print_prof_off_name_can_support_v_1_0_0_0(struct device *dev)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(g_prof_off_name_tab); i++) {
		AW_DEV_LOGE(dev, "support prof_off_name[%d]: %s",
			    i, g_prof_off_name_tab[i]);
	}
}

static int aw_parse_reg_with_hdr(struct device *dev, uint8_t *data,
				 uint32_t data_len,
				 struct aw_prof_desc *prof_desc)
{
	struct aw_bin *aw_bin = NULL;
	int ret = -1;

	aw_bin = kzalloc(data_len + sizeof(struct aw_bin), GFP_KERNEL);
	if (!aw_bin) {
		AW_DEV_LOGE(dev, "devm_kzalloc aw_bin failed");
		return -ENOMEM;
	}

	aw_bin->info.len = data_len;
	memcpy(aw_bin->info.data, data, data_len);

	ret = aw_parsing_bin_file(aw_bin);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "parse bin failed");
		kfree(aw_bin);
		return -EINVAL;
	}

	if (aw_bin->all_bin_parse_num != 1) {
		AW_DEV_LOGE(dev, "bin num[%d] error", aw_bin->all_bin_parse_num);
		kfree(aw_bin);
		return -EINVAL;
	}

	prof_desc->data_container.len = aw_bin->header_info[0].valid_data_len;
	prof_desc->data_container.data =
		data + aw_bin->header_info[0].valid_data_addr;
	prof_desc->prof_st = AW_PROFILE_OK;

	kfree(aw_bin);
	return 0;
}

static int aw_parse_data_by_sec_type_v_0_0_0_1(struct device *dev,
					       struct aw_acf_info *acf_info,
					       struct aw_dde_v_0_0_0_1 *dde,
					       struct aw_prof_desc *prof_desc)
{
	uint8_t *sec_data = (uint8_t *)acf_info->fw_data + dde->data_offset;
	int ret = 0;

	switch (dde->data_type) {
	case AW_BIN_TYPE_REG:
		snprintf(prof_desc->dev_name, AW_DEV_NAME_MAX, "%s",
			 dde->dev_name);
		if (dde->dev_profile < AW_PROFILE_MAX)
			prof_desc->prof_name = g_profile_name[dde->dev_profile];
		else
			prof_desc->prof_name = "NULL";
		prof_desc->data_container.data = sec_data;
		prof_desc->data_container.len = dde->data_size;
		prof_desc->prof_st = AW_PROFILE_OK;
		break;
	case AW_BIN_TYPE_HDR_REG:
		snprintf(prof_desc->dev_name, AW_DEV_NAME_MAX, "%s",
			 dde->dev_name);
		if (dde->dev_profile < AW_PROFILE_MAX)
			prof_desc->prof_name = g_profile_name[dde->dev_profile];
		else
			prof_desc->prof_name = "NULL";
		ret = aw_parse_reg_with_hdr(dev, sec_data, dde->data_size,
					    prof_desc);
		break;
	case AW_BIN_TYPE_MONITOR:
		ret = aw_monitor_bin_parse(dev, (const char *)sec_data,
					   dde->data_size);
		if (ret < 0)
			AW_DEV_LOGE(dev, "monitor bin parse failed");
		break;
	default:
		AW_DEV_LOGE(dev, "unsupported data_type[0x%x]", dde->data_type);
		return -EINVAL;
	}

	return ret;
}

static int aw_parse_data_by_sec_type_v_1_0_0_0(struct device *dev,
					       struct aw_acf_info *acf_info,
					       struct aw_dde_v_1_0_0_0 *dde,
					       struct aw_prof_desc *prof_desc)
{
	uint8_t *sec_data = (uint8_t *)acf_info->fw_data + dde->data_offset;
	int ret = 0;

	switch (dde->data_type) {
	case AW_BIN_TYPE_REG:
		snprintf(prof_desc->dev_name, AW_DEV_NAME_MAX, "%s",
			 dde->dev_name);
		prof_desc->prof_name = dde->dev_profile_str;
		prof_desc->data_container.data = sec_data;
		prof_desc->data_container.len = dde->data_size;
		prof_desc->prof_st = AW_PROFILE_OK;
		break;
	case AW_BIN_TYPE_HDR_REG:
		snprintf(prof_desc->dev_name, AW_DEV_NAME_MAX, "%s",
			 dde->dev_name);
		prof_desc->prof_name = dde->dev_profile_str;
		ret = aw_parse_reg_with_hdr(dev, sec_data, dde->data_size,
					    prof_desc);
		break;
	case AW_BIN_TYPE_MONITOR:
		ret = aw_monitor_bin_parse(dev, (const char *)sec_data,
					   dde->data_size);
		if (ret < 0)
			AW_DEV_LOGE(dev, "monitor bin parse failed");
		break;
	default:
		AW_DEV_LOGE(dev, "unsupported data_type[0x%x]", dde->data_type);
		return -EINVAL;
	}

	return ret;
}

static bool aw_acf_check_product_match(struct aw_acf_info *acf_info,
				       const char *dev_name)
{
	int i = 0;

	if (!acf_info->product_tab || !dev_name)
		return false;

	for (i = 0; i < acf_info->product_cnt; i++) {
		if (strncmp(acf_info->product_tab[i], dev_name,
			    AW_DEV_NAME_MAX) == 0)
			return true;
	}

	return false;
}

static int aw_acf_check_hdr(struct device *dev, struct aw_acf_info *acf_info)
{
	struct aw_acf_hdr *hdr = NULL;

	if (!acf_info->fw_data || acf_info->fw_size < sizeof(struct aw_acf_hdr)) {
		AW_DEV_LOGE(dev, "acf bin size[%d] is less than hdr size",
			    acf_info->fw_size);
		return -EINVAL;
	}

	hdr = (struct aw_acf_hdr *)acf_info->fw_data;
	if (hdr->ddt_num == 0 ||
	    hdr->ddt_offset + hdr->ddt_size > (u32)acf_info->fw_size) {
		AW_DEV_LOGE(dev, "ddt out of range");
		return -EINVAL;
	}

	memcpy(&acf_info->acf_hdr, hdr, sizeof(struct aw_acf_hdr));
	return 0;
}

static int aw_parse_acf_v_0_0_0_1(struct device *dev,
				  struct aw_acf_info *acf_info)
{
	struct aw_acf_hdr *hdr = &acf_info->acf_hdr;
	struct aw_dde_v_0_0_0_1 *dde = NULL;
	struct aw_prof_info *prof_info = &acf_info->prof_info;
	int i = 0;
	int j = 0;
	int ret = 0;
	int prof_cnt = 0;
	int dev_type_cnt = 0;
	int default_type_cnt = 0;
	bool has_off = false;

	if (hdr->ddt_size != sizeof(struct aw_dde_v_0_0_0_1)) {
		AW_DEV_LOGE(dev, "ddt_size[%d] mismatch v0.0.0.1", hdr->ddt_size);
		return -EINVAL;
	}

	dde = (struct aw_dde_v_0_0_0_1 *)(acf_info->fw_data + hdr->ddt_offset);

	for (i = 0; i < (int)hdr->ddt_num; i++) {
		if (dde[i].data_offset + dde[i].data_size > (u32)acf_info->fw_size) {
			AW_DEV_LOGE(dev, "dde[%d] data out of range", i);
			return -EINVAL;
		}
		if (dde[i].type == AW_DEV_TYPE_ID) {
			if (dde[i].dev_index == acf_info->dev_index &&
			    aw_acf_check_product_match(acf_info, dde[i].dev_name)) {
				if (dde[i].data_type == AW_BIN_TYPE_REG ||
				    dde[i].data_type == AW_BIN_TYPE_HDR_REG)
					dev_type_cnt++;
			}
		} else if (dde[i].type == AW_DEV_DEFAULT_TYPE_ID) {
			if (aw_acf_check_product_match(acf_info, dde[i].dev_name)) {
				if (dde[i].data_type == AW_BIN_TYPE_REG ||
				    dde[i].data_type == AW_BIN_TYPE_HDR_REG)
					default_type_cnt++;
			}
		}
	}

	if (dev_type_cnt > 0) {
		prof_info->prof_type = AW_DEV_TYPE_ID;
		prof_cnt = dev_type_cnt;
	} else if (default_type_cnt > 0) {
		prof_info->prof_type = AW_DEV_DEFAULT_TYPE_ID;
		prof_cnt = default_type_cnt;
	} else {
		AW_DEV_LOGE(dev, "get profile count failed");
		return -EINVAL;
	}

	prof_info->prof_desc = devm_kzalloc(dev,
					    sizeof(struct aw_prof_desc) * prof_cnt,
					    GFP_KERNEL);
	if (!prof_info->prof_desc)
		return -ENOMEM;

	prof_info->prof_name_list = devm_kzalloc(dev,
						 AW_PROFILE_STR_MAX * prof_cnt,
						 GFP_KERNEL);
	if (!prof_info->prof_name_list)
		return -ENOMEM;

	prof_info->count = prof_cnt;

	for (i = 0; i < (int)hdr->ddt_num; i++) {
		bool match = false;

		if (dde[i].type != (u32)prof_info->prof_type)
			continue;
		if (!aw_acf_check_product_match(acf_info, dde[i].dev_name))
			continue;
		if (prof_info->prof_type == AW_DEV_TYPE_ID &&
		    dde[i].dev_index != acf_info->dev_index)
			continue;

		ret = aw_crc8_check((unsigned char *)acf_info->fw_data + dde[i].data_offset,
				    dde[i].data_size, dde[i].data_crc);
		if (ret < 0) {
			AW_DEV_LOGE(dev, "dde[%d] crc8 check failed", i);
			return -EINVAL;
		}

		if (dde[i].data_type == AW_BIN_TYPE_MONITOR) {
			aw_parse_data_by_sec_type_v_0_0_0_1(dev, acf_info,
							    &dde[i], NULL);
			continue;
		}

		if (dde[i].data_type == AW_BIN_TYPE_REG ||
		    dde[i].data_type == AW_BIN_TYPE_HDR_REG)
			match = true;

		if (!match || j >= prof_cnt)
			continue;

		ret = aw_parse_data_by_sec_type_v_0_0_0_1(dev, acf_info,
							  &dde[i],
							  &prof_info->prof_desc[j]);
		if (ret < 0)
			return ret;

		snprintf(prof_info->prof_name_list[j], AW_PROFILE_STR_MAX, "%s",
			 prof_info->prof_desc[j].prof_name);
		if (aw_check_prof_str_is_off(prof_info->prof_name_list[j]) == 0)
			has_off = true;
		j++;
	}

	if (!has_off) {
		AW_DEV_LOGE(dev, "not found profile off");
		return -EINVAL;
	}

	prof_info->status = AW_ACF_UPDATE;
	return 0;
}

static int aw_parse_acf_v_1_0_0_0(struct device *dev,
				  struct aw_acf_info *acf_info)
{
	struct aw_acf_hdr *hdr = &acf_info->acf_hdr;
	struct aw_dde_v_1_0_0_0 *dde = NULL;
	struct aw_prof_info *prof_info = &acf_info->prof_info;
	int i = 0;
	int j = 0;
	int ret = 0;
	int prof_cnt = 0;
	int dev_type_cnt = 0;
	int default_type_cnt = 0;
	bool has_off = false;

	if (hdr->ddt_size != sizeof(struct aw_dde_v_1_0_0_0) &&
	    hdr->ddt_size != hdr->ddt_num * sizeof(struct aw_dde_v_1_0_0_0)) {
		AW_DEV_LOGE(dev, "ddt_size[%d] mismatch v1.0.0.0", hdr->ddt_size);
		return -EINVAL;
	}

	dde = (struct aw_dde_v_1_0_0_0 *)(acf_info->fw_data + hdr->ddt_offset);

	for (i = 0; i < (int)hdr->ddt_num; i++) {
		if (dde[i].data_offset + dde[i].data_size > (u32)acf_info->fw_size) {
			AW_DEV_LOGE(dev, "dde[%d] data out of range", i);
			return -EINVAL;
		}
		if (dde[i].type == AW_DEV_TYPE_ID) {
			if (dde[i].dev_index == acf_info->dev_index &&
			    (dde[i].chip_id == acf_info->aw_dev->chipid ||
			     aw_acf_check_product_match(acf_info, dde[i].dev_name))) {
				if (dde[i].data_type == AW_BIN_TYPE_REG ||
				    dde[i].data_type == AW_BIN_TYPE_HDR_REG)
					dev_type_cnt++;
			}
		} else if (dde[i].type == AW_DEV_DEFAULT_TYPE_ID) {
			if (dde[i].chip_id == acf_info->aw_dev->chipid ||
			    aw_acf_check_product_match(acf_info, dde[i].dev_name)) {
				if (dde[i].data_type == AW_BIN_TYPE_REG ||
				    dde[i].data_type == AW_BIN_TYPE_HDR_REG)
					default_type_cnt++;
			}
		}
	}

	if (dev_type_cnt > 0) {
		prof_info->prof_type = AW_DEV_TYPE_ID;
		prof_cnt = dev_type_cnt;
	} else if (default_type_cnt > 0) {
		prof_info->prof_type = AW_DEV_DEFAULT_TYPE_ID;
		prof_cnt = default_type_cnt;
	} else {
		AW_DEV_LOGE(dev, "get profile count failed");
		return -EINVAL;
	}

	prof_info->prof_desc = devm_kzalloc(dev,
					    sizeof(struct aw_prof_desc) * prof_cnt,
					    GFP_KERNEL);
	if (!prof_info->prof_desc)
		return -ENOMEM;

	prof_info->prof_name_list = devm_kzalloc(dev,
						 AW_PROFILE_STR_MAX * prof_cnt,
						 GFP_KERNEL);
	if (!prof_info->prof_name_list)
		return -ENOMEM;

	prof_info->count = prof_cnt;

	for (i = 0; i < (int)hdr->ddt_num; i++) {
		if (dde[i].type != (u32)prof_info->prof_type)
			continue;
		if (dde[i].chip_id != acf_info->aw_dev->chipid &&
		    !aw_acf_check_product_match(acf_info, dde[i].dev_name))
			continue;
		if (prof_info->prof_type == AW_DEV_TYPE_ID &&
		    dde[i].dev_index != acf_info->dev_index)
			continue;

		ret = aw_crc8_check((unsigned char *)acf_info->fw_data + dde[i].data_offset,
				    dde[i].data_size, dde[i].data_crc);
		if (ret < 0) {
			AW_DEV_LOGE(dev, "dde[%d] crc8 check failed", i);
			return -EINVAL;
		}

		if (dde[i].data_type == AW_BIN_TYPE_MONITOR) {
			aw_parse_data_by_sec_type_v_1_0_0_0(dev, acf_info,
							    &dde[i], NULL);
			continue;
		}

		if (dde[i].data_type != AW_BIN_TYPE_REG &&
		    dde[i].data_type != AW_BIN_TYPE_HDR_REG)
			continue;

		if (j >= prof_cnt)
			continue;

		ret = aw_parse_data_by_sec_type_v_1_0_0_0(dev, acf_info,
							  &dde[i],
							  &prof_info->prof_desc[j]);
		if (ret < 0)
			return ret;

		snprintf(prof_info->prof_name_list[j], AW_PROFILE_STR_MAX, "%s",
			 dde[i].dev_profile_str);
		prof_info->prof_desc[j].prof_name = prof_info->prof_name_list[j];
		if (aw_check_prof_str_is_off(prof_info->prof_name_list[j]) == 0)
			has_off = true;
		j++;
	}

	if (!has_off) {
		aw_print_prof_off_name_can_support_v_1_0_0_0(dev);
		return -EINVAL;
	}

	prof_info->status = AW_ACF_UPDATE;
	return 0;
}

void aw_acf_profile_free(struct device *dev, struct aw_acf_info *acf_info)
{
	memset(&acf_info->acf_hdr, 0, sizeof(struct aw_acf_hdr));
	acf_info->prof_info.count = 0;
	acf_info->prof_info.status = AW_ACF_WAIT;

	if (acf_info->prof_info.prof_desc) {
		devm_kfree(dev, acf_info->prof_info.prof_desc);
		acf_info->prof_info.prof_desc = NULL;
	}

	if (acf_info->prof_info.prof_name_list) {
		devm_kfree(dev, acf_info->prof_info.prof_name_list);
		acf_info->prof_info.prof_name_list = NULL;
	}

	if (acf_info->fw_data) {
		vfree(acf_info->fw_data);
		acf_info->fw_data = NULL;
	}
}

int aw_acf_parse(struct device *dev, struct aw_acf_info *acf_info)
{
	int ret = 0;

	ret = aw_acf_check_hdr(dev, acf_info);
	if (ret < 0)
		goto err_free;

	switch (acf_info->acf_hdr.hdr_version) {
	case AW_ACF_HDR_VERSION_0_0_0_1:
		ret = aw_parse_acf_v_0_0_0_1(dev, acf_info);
		break;
	case AW_ACF_HDR_VERSION_1_0_0_0:
		ret = aw_parse_acf_v_1_0_0_0(dev, acf_info);
		break;
	default:
		AW_DEV_LOGE(dev, "unsupported acf hdr_version[0x%x]",
			    acf_info->acf_hdr.hdr_version);
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		goto err_free;

	return 0;

err_free:
	aw_acf_profile_free(dev, acf_info);
	return ret;
}

struct aw_prof_desc *aw_acf_get_prof_desc_form_name(struct device *dev,
						    struct aw_acf_info *acf_info,
						    char *profile_name)
{
	int i = 0;
	struct aw_prof_info *prof_info = &acf_info->prof_info;

	if (!prof_info->status) {
		AW_DEV_LOGE(dev, "profile_cfg not load");
		return NULL;
	}

	for (i = 0; i < prof_info->count; i++) {
		if (!strncmp(profile_name, prof_info->prof_desc[i].prof_name,
			     AW_PROFILE_STR_MAX))
			return &prof_info->prof_desc[i];
	}

	AW_DEV_LOGE(dev, "not found profile[%s]", profile_name);
	return NULL;
}

int aw_acf_get_prof_index_form_name(struct device *dev,
				    struct aw_acf_info *acf_info,
				    char *profile_name)
{
	int i = 0;
	struct aw_prof_info *prof_info = &acf_info->prof_info;

	if (!prof_info->status) {
		AW_DEV_LOGE(dev, "profile_cfg not load");
		return -EINVAL;
	}

	for (i = 0; i < prof_info->count; i++) {
		if (!strncmp(profile_name, prof_info->prof_name_list[i],
			     AW_PROFILE_STR_MAX))
			return i;
	}

	AW_DEV_LOGE(dev, "profile_index not found");
	return -EINVAL;
}

char *aw_acf_get_prof_name_form_index(struct device *dev,
				      struct aw_acf_info *acf_info, int index)
{
	struct aw_prof_info *prof_info = &acf_info->prof_info;

	if (!prof_info->status) {
		AW_DEV_LOGE(dev, "profile_cfg not load");
		return NULL;
	}

	if (index < 0 || index >= prof_info->count) {
		AW_DEV_LOGE(dev, "profile_index out of table");
		return NULL;
	}

	return prof_info->prof_desc[index].prof_name;
}

int aw_acf_get_profile_count(struct device *dev, struct aw_acf_info *acf_info)
{
	struct aw_prof_info *prof_info = &acf_info->prof_info;

	if (!prof_info->status) {
		AW_DEV_LOGE(dev, "profile_cfg not load");
		return -EINVAL;
	}

	return prof_info->count;
}

char *aw_acf_get_prof_off_name(struct device *dev, struct aw_acf_info *acf_info)
{
	int i = 0;
	struct aw_prof_info *prof_info = &acf_info->prof_info;

	if (!prof_info->status) {
		AW_DEV_LOGE(dev, "profile_cfg not load");
		return NULL;
	}

	for (i = 0; i < prof_info->count; i++) {
		if (aw_check_prof_str_is_off(prof_info->prof_name_list[i]) == 0)
			return prof_info->prof_name_list[i];
	}

	return NULL;
}

void aw_acf_init(struct aw_device *aw_dev, struct aw_acf_info *acf_info,
		 int dev_index)
{
	acf_info->fw_load_retry_cnt = 0;
	acf_info->prof_info.status = AW_ACF_WAIT;
	acf_info->dev_index = (u16)dev_index;
	acf_info->aw_dev = aw_dev;
	acf_info->product_cnt = aw_dev->product_cnt;
	acf_info->product_tab = aw_dev->product_tab;
	acf_info->prof_info.prof_desc = NULL;
	acf_info->fw_data = NULL;
	acf_info->fw_size = 0;
}
