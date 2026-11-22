/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "aw87xxx.h"
#include "aw87xxx_log.h"
#include "aw87xxx_pid_reg.h"

struct aw_componet_codec_ops {
	int (*add_codec_controls)(struct snd_soc_component *component,
				  const struct snd_kcontrol_new *controls,
				  unsigned int num_controls);
};

static const struct aw_componet_codec_ops aw_componet_codec_ops = {
	.add_codec_controls = snd_soc_add_component_controls,
};

static LIST_HEAD(g_aw87xxx_list);
static DEFINE_MUTEX(g_aw87xxx_mutex_lock);
static unsigned int g_aw87xxx_dev_cnt;

static int aw87xxx_update_off_prof(struct aw87xxx *aw87xxx, char *profile)
{
	struct aw_prof_desc *prof_desc = NULL;
	int ret = 0;

	mutex_lock(&aw87xxx->reg_lock);
	aw_monitor_stop(&aw87xxx->monitor);

	prof_desc = aw_acf_get_prof_desc_form_name(aw87xxx->dev,
						   &aw87xxx->acf_info,
						   profile);
	if (!prof_desc || !prof_desc->prof_st) {
		aw_dev_hw_pwr_ctrl(&aw87xxx->aw_dev, false);
		aw87xxx->current_profile = aw87xxx->prof_off_name;
		mutex_unlock(&aw87xxx->reg_lock);
		return 0;
	}

	if (aw87xxx->aw_dev.hwen_status == AW_DEV_HWEN_OFF) {
		AW_DEV_LOGI(aw87xxx->dev,
			    "profile[%s] has already load ", profile);
		aw87xxx->current_profile = prof_desc->prof_name;
		mutex_unlock(&aw87xxx->reg_lock);
		return 0;
	}

	if (aw87xxx->aw_dev.ops.pwr_off_func) {
		ret = aw87xxx->aw_dev.ops.pwr_off_func(&aw87xxx->aw_dev,
						       &prof_desc->data_container);
	} else {
		ret = aw_dev_default_pwr_off(&aw87xxx->aw_dev,
					     &prof_desc->data_container);
	}

	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "load profile[%s] failed ", profile);
		mutex_unlock(&aw87xxx->reg_lock);
		return ret;
	}

	aw87xxx->current_profile = prof_desc->prof_name;
	mutex_unlock(&aw87xxx->reg_lock);
	return 0;
}

int aw87xxx_update_profile(struct aw87xxx *aw87xxx, char *profile)
{
	struct aw_prof_desc *prof_desc = NULL;
	int ret = 0;

	if (!aw87xxx->acf_info.prof_info.status) {
		AW_DEV_LOGE(aw87xxx->dev, "profile_cfg not load");
		return -EINVAL;
	}

	if (!strncmp(profile, aw87xxx->prof_off_name, AW_PROFILE_STR_MAX))
		return aw87xxx_update_off_prof(aw87xxx, profile);

	mutex_lock(&aw87xxx->reg_lock);

	prof_desc = aw_acf_get_prof_desc_form_name(aw87xxx->dev,
						   &aw87xxx->acf_info,
						   profile);
	if (!prof_desc) {
		AW_DEV_LOGE(aw87xxx->dev, "not found [%s] parameter", profile);
		mutex_unlock(&aw87xxx->reg_lock);
		return -EINVAL;
	}

	if (!prof_desc->prof_st) {
		AW_DEV_LOGE(aw87xxx->dev, "not found data container");
		mutex_unlock(&aw87xxx->reg_lock);
		return -EINVAL;
	}

	aw_monitor_stop(&aw87xxx->monitor);

	if (aw87xxx->aw_dev.ops.pwr_on_func) {
		ret = aw87xxx->aw_dev.ops.pwr_on_func(&aw87xxx->aw_dev,
						      &prof_desc->data_container);
	} else {
		ret = aw_dev_default_pwr_on(&aw87xxx->aw_dev,
					    &prof_desc->data_container);
	}

	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "load profile[%s] failed ", profile);
		mutex_unlock(&aw87xxx->reg_lock);
		return aw87xxx_update_off_prof(aw87xxx, aw87xxx->prof_off_name);
	}

	aw87xxx->current_profile = prof_desc->prof_name;
	aw_monitor_start(&aw87xxx->monitor);
	mutex_unlock(&aw87xxx->reg_lock);
	return 0;
}

int aw87xxx_esd_update_profile(struct aw87xxx *aw87xxx, char *profile)
{
	struct aw_prof_desc *prof_desc = NULL;
	int ret = 0;

	if (!aw87xxx->acf_info.prof_info.status) {
		AW_DEV_LOGE(aw87xxx->dev, "profile_cfg not load");
		return -EINVAL;
	}

	mutex_lock(&aw87xxx->reg_lock);

	prof_desc = aw_acf_get_prof_desc_form_name(aw87xxx->dev,
						   &aw87xxx->acf_info,
						   profile);
	if (!prof_desc) {
		AW_DEV_LOGE(aw87xxx->dev, "not found [%s] parameter", profile);
		mutex_unlock(&aw87xxx->reg_lock);
		return -EINVAL;
	}

	if (!prof_desc->prof_st) {
		AW_DEV_LOGE(aw87xxx->dev, "not found data container");
		mutex_unlock(&aw87xxx->reg_lock);
		return -EINVAL;
	}

	if (!strncmp(profile, aw87xxx->prof_off_name, AW_PROFILE_STR_MAX)) {
		if (aw87xxx->aw_dev.ops.pwr_off_func) {
			ret = aw87xxx->aw_dev.ops.pwr_off_func(
				&aw87xxx->aw_dev, &prof_desc->data_container);
		} else {
			ret = aw_dev_default_pwr_off(&aw87xxx->aw_dev,
						     &prof_desc->data_container);
		}
		if (ret < 0) {
			AW_DEV_LOGE(aw87xxx->dev,
				    "load profile[%s] failed ", profile);
			mutex_unlock(&aw87xxx->reg_lock);
			return ret;
		}
		aw87xxx->current_profile = prof_desc->prof_name;
		mutex_unlock(&aw87xxx->reg_lock);
		return 0;
	}

	if (aw87xxx->aw_dev.ops.pwr_on_func) {
		ret = aw87xxx->aw_dev.ops.pwr_on_func(&aw87xxx->aw_dev,
						      &prof_desc->data_container);
	} else {
		ret = aw_dev_default_pwr_on(&aw87xxx->aw_dev,
					    &prof_desc->data_container);
	}

	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "load profile[%s] failed ", profile);
		mutex_unlock(&aw87xxx->reg_lock);
		return ret;
	}

	aw87xxx->current_profile = prof_desc->prof_name;
	mutex_unlock(&aw87xxx->reg_lock);
	return 0;
}

char *aw87xxx_show_current_profile(int dev_index)
{
	struct list_head *pos = NULL;
	struct aw87xxx *aw87xxx = NULL;

	list_for_each(pos, &g_aw87xxx_list) {
		aw87xxx = list_entry(pos, struct aw87xxx, list);
		if (!aw87xxx) {
			AW_LOGE("struct aw87xxx not ready");
			return NULL;
		}
		if (aw87xxx->dev_index == dev_index) {
			AW_DEV_LOGI(aw87xxx->dev, "current profile is [%s]",
				    aw87xxx->current_profile);
			return aw87xxx->current_profile;
		}
	}

	AW_LOGE("not found struct aw87xxx, dev_index = [%d]", dev_index);
	return NULL;
}
EXPORT_SYMBOL(aw87xxx_show_current_profile);

int aw87xxx_set_profile(int dev_index, char *profile)
{
	struct list_head *pos = NULL;
	struct aw87xxx *aw87xxx = NULL;

	list_for_each(pos, &g_aw87xxx_list) {
		aw87xxx = list_entry(pos, struct aw87xxx, list);
		if (!aw87xxx) {
			AW_LOGE("struct aw87xxx not ready");
			return -EINVAL;
		}
		if (profile && aw87xxx->dev_index == dev_index)
			return aw87xxx_update_profile(aw87xxx, profile);
	}

	AW_LOGE("not found struct aw87xxx, dev_index = [%d]", dev_index);
	return -EINVAL;
}
EXPORT_SYMBOL(aw87xxx_set_profile);

/* ALSA SoC kcontrol callbacks */
static int aw87xxx_profile_switch_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	struct aw87xxx *aw87xxx = (struct aw87xxx *)kcontrol->private_value;
	int count = 0;
	char *name = NULL;

	if (!aw87xxx) {
		AW_LOGE("get struct aw87xxx failed");
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;

	count = aw_acf_get_profile_count(aw87xxx->dev, &aw87xxx->acf_info);
	if (count <= 0) {
		uinfo->value.enumerated.items = 0;
		AW_DEV_LOGE(aw87xxx->dev, "get count[%d] failed", count);
		return 0;
	}

	uinfo->value.enumerated.items = count;
	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	name = aw_acf_get_prof_name_form_index(aw87xxx->dev, &aw87xxx->acf_info,
					       uinfo->value.enumerated.item);
	if (!name)
		strscpy(uinfo->value.enumerated.name, "NULL",
			sizeof(uinfo->value.enumerated.name));
	else
		strscpy(uinfo->value.enumerated.name, name,
			sizeof(uinfo->value.enumerated.name));

	return 0;
}

static int aw87xxx_profile_switch_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct aw87xxx *aw87xxx = (struct aw87xxx *)kcontrol->private_value;
	int index = 0;

	if (!aw87xxx) {
		AW_LOGE("get struct aw87xxx failed");
		return -EINVAL;
	}

	if (!aw87xxx->current_profile) {
		AW_DEV_LOGE(aw87xxx->dev, "current profile is NULL");
		return -EINVAL;
	}

	AW_DEV_LOGI(aw87xxx->dev, "current profile:[%s]",
		    aw87xxx->current_profile);

	index = aw_acf_get_prof_index_form_name(aw87xxx->dev,
						&aw87xxx->acf_info,
						aw87xxx->current_profile);
	if (index < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "get profile index failed");
		return index;
	}

	ucontrol->value.integer.value[0] = index;
	return 0;
}

static int aw87xxx_profile_switch_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct aw87xxx *aw87xxx = (struct aw87xxx *)kcontrol->private_value;
	int index = 0;
	char *profile = NULL;
	int ret = 0;

	if (!aw87xxx) {
		AW_LOGE("get struct aw87xxx failed");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	profile = aw_acf_get_prof_name_form_index(aw87xxx->dev,
						  &aw87xxx->acf_info, index);
	if (!profile) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "not found profile name,index=[%d]", index);
		return -EINVAL;
	}

	AW_DEV_LOGI(aw87xxx->dev, "set profile [%s]", profile);
	ret = aw87xxx_update_profile(aw87xxx, profile);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "set dev_index[%d] profile failed, profile = %s",
			    aw87xxx->dev_index, profile);
		return ret;
	}

	return 0;
}

static int aw87xxx_vmax_get_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = INT_MIN;
	uinfo->value.integer.max = 0;
	return 0;
}

static int aw87xxxx_vmax_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct aw87xxx *aw87xxx = (struct aw87xxx *)kcontrol->private_value;
	int vmax_val = 0;
	int ret = 0;

	if (!aw87xxx) {
		AW_LOGE("get struct aw87xxx failed");
		return -EINVAL;
	}

	ret = aw_monitor_no_dsp_get_vmax(&aw87xxx->monitor, &vmax_val);
	if (ret < 0)
		return ret;

	ucontrol->value.integer.value[0] = vmax_val;
	AW_DEV_LOGI(aw87xxx->dev, "get vmax = [0x%x]", vmax_val);
	return 0;
}

static int aw87xxx_kcontrol_dynamic_create(struct aw87xxx *aw87xxx, void *codec)
{
	struct snd_kcontrol_new *aw87xxx_kcontrol = NULL;
	char *kctl_name = NULL;
	int ret = 0;

	aw87xxx->codec = (struct snd_soc_component *)codec;

	aw87xxx_kcontrol = devm_kzalloc(aw87xxx->dev,
					sizeof(struct snd_kcontrol_new) *
					AW87XXX_KCONTROL_NUM,
					GFP_KERNEL);
	if (!aw87xxx_kcontrol) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "aw87xxx_kcontrol devm_kzalloc failed");
		return -ENOMEM;
	}

	kctl_name = devm_kzalloc(aw87xxx->dev, AW_PROFILE_STR_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;
	snprintf(kctl_name, AW_PROFILE_STR_MAX, "aw87xxx_profile_switch_%d",
		 aw87xxx->dev_index);

	aw87xxx_kcontrol[0].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw87xxx_kcontrol[0].name = kctl_name;
	aw87xxx_kcontrol[0].info = aw87xxx_profile_switch_info;
	aw87xxx_kcontrol[0].get = aw87xxx_profile_switch_get;
	aw87xxx_kcontrol[0].put = aw87xxx_profile_switch_put;
	aw87xxx_kcontrol[0].private_value = (unsigned long)aw87xxx;

	kctl_name = devm_kzalloc(aw87xxx->dev, AW_PROFILE_STR_MAX, GFP_KERNEL);
	if (!kctl_name)
		return -ENOMEM;
	snprintf(kctl_name, AW_PROFILE_STR_MAX, "aw87xxx_vmax_get_%d",
		 aw87xxx->dev_index);

	aw87xxx_kcontrol[1].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	aw87xxx_kcontrol[1].name = kctl_name;
	aw87xxx_kcontrol[1].access = SNDRV_CTL_ELEM_ACCESS_READ;
	aw87xxx_kcontrol[1].info = aw87xxx_vmax_get_info;
	aw87xxx_kcontrol[1].get = aw87xxxx_vmax_get;
	aw87xxx_kcontrol[1].private_value = (unsigned long)aw87xxx;

	ret = aw_componet_codec_ops.add_codec_controls(aw87xxx->codec,
						       aw87xxx_kcontrol,
						       AW87XXX_KCONTROL_NUM);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "add codec controls failed, ret = %d", ret);
		return ret;
	}

	AW_DEV_LOGI(aw87xxx->dev, "add codec controls[%s,%s]",
		    aw87xxx_kcontrol[0].name, aw87xxx_kcontrol[1].name);
	return 0;
}

int aw87xxx_add_codec_controls(void *codec)
{
	struct list_head *pos = NULL;
	struct aw87xxx *aw87xxx = NULL;
	int ret = 0;

	list_for_each(pos, &g_aw87xxx_list) {
		aw87xxx = list_entry(pos, struct aw87xxx, list);
		if (!aw87xxx) {
			AW_LOGE("struct aw87xxx not ready");
			return -1;
		}
		ret = aw87xxx_kcontrol_dynamic_create(aw87xxx, codec);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(aw87xxx_add_codec_controls);

/* Sysfs attributes */
static ssize_t aw87xxx_attr_get_reg(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;
	u8 i = 0;
	u8 reg_val = 0;

	for (i = 0; i < aw87xxx->aw_dev.reg_max_addr; i++) {
		if (!(aw87xxx->aw_dev.reg_access[i] & REG_RD_ACCESS))
			continue;
		aw_dev_i2c_read_byte(&aw87xxx->aw_dev, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02x=0x%02x\n", i, reg_val);
	}

	return len;
}

static ssize_t aw87xxx_attr_set_reg(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	unsigned int databuf[2] = { 0, 0 };
	int ret = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		AW_DEV_LOGE(aw87xxx->dev, "i2c write cmd input error");
		return len;
	}

	if (databuf[0] >= (unsigned int)aw87xxx->aw_dev.reg_max_addr) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "set reg[0x%x] error,is out of reg_addr_max[0x%x]",
			    databuf[0], aw87xxx->aw_dev.reg_max_addr);
		return -EINVAL;
	}

	ret = aw_dev_i2c_write_byte(&aw87xxx->aw_dev, (u8)databuf[0],
				    (u8)databuf[1]);
	if (ret < 0)
		AW_DEV_LOGE(aw87xxx->dev, "set [0x%x]=0x%x failed",
			    databuf[0], databuf[1]);

	return len;
}

static ssize_t aw87xxx_attr_get_profile(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i = 0;

	if (!aw87xxx->acf_info.prof_info.status) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"profile_cfg not load\n");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "profile: %s\n",
			aw87xxx->current_profile ? aw87xxx->current_profile :
						   "NULL");

	for (i = 0; i < aw87xxx->acf_info.prof_info.count; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "[%d]: %s\n",
				i, aw87xxx->acf_info.prof_info.prof_name_list[i]);
	}

	return len;
}

static ssize_t aw87xxx_attr_set_profile(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	char profile[AW_PROFILE_STR_MAX] = { 0 };
	int ret = 0;

	if (sscanf(buf, "%31s", profile) != 1)
		return -EINVAL;

	ret = aw87xxx_update_profile(aw87xxx, profile);
	if (ret < 0)
		return ret;

	return len;
}

static ssize_t aw87xxx_attr_get_hwen(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "hwen: %d\n",
			aw87xxx->aw_dev.hwen_status);
	return len;
}

static ssize_t aw87xxx_attr_set_hwen(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	unsigned int state = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &state);
	if (ret < 0)
		return ret;

	if (state == AW_DEV_HWEN_ON)
		aw_dev_hw_pwr_ctrl(&aw87xxx->aw_dev, true);
	else
		aw_dev_hw_pwr_ctrl(&aw87xxx->aw_dev, false);

	return len;
}

int aw87xxx_awrw_write(struct aw87xxx *aw87xxx, const char *buf)
{
	int i = 0;
	int ret = 0;
	int data_len = aw87xxx->awrw_ctrl.reg_num;
	const char *p_data = buf + 25;
	unsigned int reg_val = 0;
	u8 *reg_data = NULL;

	reg_data = vmalloc(data_len);
	if (!reg_data) {
		AW_DEV_LOGE(aw87xxx->dev, "memory alloc failed");
		return -ENOMEM;
	}

	for (i = 0; i < data_len; i++) {
		if (sscanf(p_data + i * 5, "0x%02x", &reg_val) != 1) {
			AW_DEV_LOGE(aw87xxx->dev, "parse write data failed");
			vfree(reg_data);
			return -EINVAL;
		}
		reg_data[i] = (u8)reg_val;
	}

	mutex_lock(&aw87xxx->reg_lock);
	for (i = 0; i < data_len; i++) {
		ret = aw_dev_i2c_write_byte(&aw87xxx->aw_dev,
					    (u8)(aw87xxx->awrw_ctrl.reg_addr + i),
					    reg_data[i]);
		if (ret < 0)
			break;
	}
	mutex_unlock(&aw87xxx->reg_lock);

	vfree(reg_data);
	return ret;
}

static ssize_t aw87xxx_attr_awrw_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i = 0;
	int ret = 0;
	u8 *reg_data = NULL;

	if (aw87xxx->awrw_ctrl.status != AW87XXX_AWRW_READ) {
		AW_DEV_LOGE(aw87xxx->dev, "please write read cmd first");
		return -EINVAL;
	}

	reg_data = vmalloc(aw87xxx->awrw_ctrl.reg_num);
	if (!reg_data) {
		AW_DEV_LOGE(aw87xxx->dev, "memory alloc failed");
		aw87xxx->awrw_ctrl.status = AW87XXX_AWRW_WRITE;
		return -EINVAL;
	}

	mutex_lock(&aw87xxx->reg_lock);
	ret = aw_dev_i2c_read_msg(&aw87xxx->aw_dev,
				  (u8)aw87xxx->awrw_ctrl.reg_addr,
				  reg_data, (u32)aw87xxx->awrw_ctrl.reg_num);
	mutex_unlock(&aw87xxx->reg_lock);

	if (ret < 0) {
		vfree(reg_data);
		aw87xxx->awrw_ctrl.status = AW87XXX_AWRW_WRITE;
		return -EFAULT;
	}

	AW_DEV_LOGI(aw87xxx->dev, "reg_addr 0x%02x, reg_num %d",
		    aw87xxx->awrw_ctrl.reg_addr, aw87xxx->awrw_ctrl.reg_num);

	for (i = 0; i < aw87xxx->awrw_ctrl.reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,",
				reg_data[i]);
		AW_DEV_LOGI(aw87xxx->dev, "0x%02x", reg_data[i]);
	}

	vfree(reg_data);
	aw87xxx->awrw_ctrl.status = AW87XXX_AWRW_WRITE;
	return len;
}

static int aw87xxx_awrw_data_check(struct aw87xxx *aw87xxx,
				   unsigned int addr_bytes,
				   unsigned int data_bytes,
				   unsigned int rw_status,
				   unsigned int reg_num, size_t count)
{
	int req_len = 0;
	int act_len = 0;

	if (addr_bytes != 1 || data_bytes != 1) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "addr_bytes [%d] or data_bytes [%d] unsupport",
			    addr_bytes, data_bytes);
		return -EINVAL;
	}

	if (rw_status == AW87XXX_AWRW_WRITE) {
		req_len = reg_num * 5;
		act_len = (int)count - 25;
		if (req_len > act_len) {
			AW_DEV_LOGE(aw87xxx->dev,
				    "data_len checkfailed,requeset data_len [%d],actaul data_len [%d]",
				    req_len, act_len);
			return -EINVAL;
		}
	}

	return 0;
}

static ssize_t aw87xxx_attr_awrw_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	unsigned int rw_status = 0;
	unsigned int addr_bytes = 0;
	unsigned int data_bytes = 0;
	unsigned int reg_num = 0;
	unsigned int reg_addr = 0;
	int ret = 0;

	if (count < 24) {
		AW_DEV_LOGE(aw87xxx->dev,
			    "data count too smaller, please check write format");
		AW_DEV_LOGE(aw87xxx->dev, "string %s,count=%ld", buf, count);
		return -EINVAL;
	}

	AW_DEV_LOGI(aw87xxx->dev, "string:[%s],count=%ld", buf, count);

	if (sscanf(buf, "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		   &rw_status, &addr_bytes, &data_bytes,
		   &reg_num, &reg_addr) != 5) {
		AW_DEV_LOGE(aw87xxx->dev, "can not parse string");
		return -EINVAL;
	}

	aw87xxx->awrw_ctrl.reg_addr = reg_addr;
	aw87xxx->awrw_ctrl.reg_num = reg_num;

	ret = aw87xxx_awrw_data_check(aw87xxx, addr_bytes, data_bytes,
				      rw_status, reg_num, count);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "can not parse string");
		return -EINVAL;
	}

	if (rw_status == AW87XXX_AWRW_WRITE) {
		ret = aw87xxx_awrw_write(aw87xxx, buf);
		if (ret < 0)
			return ret;
	} else if (rw_status == AW87XXX_AWRW_READ) {
		aw87xxx->awrw_ctrl.status = AW87XXX_AWRW_READ;
		AW_DEV_LOGI(aw87xxx->dev,
			    "read_cmd:reg_addr[0x%02x], reg_num[%d]",
			    reg_addr, reg_num);
	} else {
		AW_DEV_LOGE(aw87xxx->dev,
			    "please check str format, unsupport read_write_status: %d",
			    rw_status);
		return -EINVAL;
	}

	return count;
}

static ssize_t aw87xxx_drv_ver_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "driver_ver: %s\n",
			AW87XXX_DRIVER_VERSION);
	return len;
}

static DEVICE_ATTR(reg, 0644, aw87xxx_attr_get_reg, aw87xxx_attr_set_reg);
static DEVICE_ATTR(profile, 0644,
		   aw87xxx_attr_get_profile, aw87xxx_attr_set_profile);
static DEVICE_ATTR(hwen, 0644, aw87xxx_attr_get_hwen, aw87xxx_attr_set_hwen);
static DEVICE_ATTR(awrw, 0644, aw87xxx_attr_awrw_show, aw87xxx_attr_awrw_store);
static DEVICE_ATTR(drv_ver, 0444, aw87xxx_drv_ver_show, NULL);

static struct attribute *aw87xxx_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_profile.attr,
	&dev_attr_hwen.attr,
	&dev_attr_awrw.attr,
	&dev_attr_drv_ver.attr,
	NULL,
};

static struct attribute_group aw87xxx_attribute_group = {
	.attrs = aw87xxx_attributes,
};

/* Firmware loading */
static void aw87xxx_fw_load_retry(struct aw87xxx *aw87xxx)
{
	if (aw87xxx->acf_info.fw_load_retry_cnt < AW87XXX_LOAD_FW_RETRY_MAX) {
		schedule_delayed_work(&aw87xxx->fw_load_work,
				      msecs_to_jiffies(AW87XXX_LOAD_FW_RETRY_DELAY_MS));
		aw87xxx->acf_info.fw_load_retry_cnt++;
	} else {
		aw87xxx->acf_info.fw_load_retry_cnt = 0;
		AW_DEV_LOGE(aw87xxx->dev,
			    "can not load firmware,please check name or file exists");
	}
}

static int aw87xxx_init_default_prof(struct aw87xxx *aw87xxx)
{
	char *off_name = NULL;

	off_name = aw_acf_get_prof_off_name(aw87xxx->dev, &aw87xxx->acf_info);
	if (!off_name) {
		AW_DEV_LOGE(aw87xxx->dev, "get profile off name failed");
		return -EINVAL;
	}

	snprintf(aw87xxx->prof_off_name, AW_PROFILE_STR_MAX, "%s", off_name);
	aw87xxx->current_profile = off_name;
	AW_DEV_LOGI(aw87xxx->dev, "init profile name [%s]", off_name);
	return 0;
}

static void aw87xxx_fw_load(const struct firmware *fw, void *context)
{
	struct aw87xxx *aw87xxx = (struct aw87xxx *)context;
	int ret = 0;

	if (!fw) {
		aw87xxx_fw_load_retry(aw87xxx);
		return;
	}

	mutex_lock(&aw87xxx->reg_lock);

	aw87xxx->acf_info.fw_data = vmalloc(fw->size);
	if (!aw87xxx->acf_info.fw_data) {
		AW_DEV_LOGE(aw87xxx->dev, "fw_data kzalloc memory failed");
		goto err_release;
	}

	memset(aw87xxx->acf_info.fw_data, 0, fw->size);
	memcpy(aw87xxx->acf_info.fw_data, fw->data, fw->size);
	aw87xxx->acf_info.fw_size = (int)fw->size;

	ret = aw_acf_parse(aw87xxx->dev, &aw87xxx->acf_info);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "fw_data parse failed");
		goto err_release;
	}

	ret = aw87xxx_init_default_prof(aw87xxx);
	if (ret < 0) {
		aw_acf_profile_free(aw87xxx->dev, &aw87xxx->acf_info);
		aw_monitor_cfg_free(&aw87xxx->monitor);
		goto err_release;
	}

	AW_DEV_LOGI(aw87xxx->dev, "acf parse succeed");
	mutex_unlock(&aw87xxx->reg_lock);
	release_firmware(fw);
	return;

err_release:
	release_firmware(fw);
	mutex_unlock(&aw87xxx->reg_lock);
}

static void aw87xxx_fw_load_work_routine(struct work_struct *work)
{
	struct aw87xxx *aw87xxx = container_of(work, struct aw87xxx,
					      fw_load_work.work);

	if (aw87xxx->acf_info.prof_info.status == AW_ACF_UPDATE)
		return;

	request_firmware_nowait(THIS_MODULE, FW_ACTION_UEVENT,
				aw87xxx->fw_name, aw87xxx->dev, GFP_KERNEL,
				aw87xxx, aw87xxx_fw_load);
}

static void aw87xxx_fw_load_init(struct aw87xxx *aw87xxx)
{
	AW_DEV_LOGI(aw87xxx->dev, "enter");

	aw_acf_init(&aw87xxx->aw_dev, &aw87xxx->acf_info, aw87xxx->dev_index);
	snprintf(aw87xxx->fw_name, AW87XXX_FW_NAME_MAX, "%s",
		 AW87XXX_FW_NAME_DEFAULT);

	INIT_DELAYED_WORK(&aw87xxx->fw_load_work, aw87xxx_fw_load_work_routine);
	schedule_delayed_work(&aw87xxx->fw_load_work,
			      msecs_to_jiffies(AW87XXX_LOAD_FW_DELAY_MS));
}

/* DT parsing and I2C probe/remove/shutdown */
int aw87xxx_dtsi_dev_index_check(struct aw87xxx *aw87xxx)
{
	struct list_head *pos = NULL;
	struct aw87xxx *dev_item = NULL;

	list_for_each(pos, &g_aw87xxx_list) {
		dev_item = list_entry(pos, struct aw87xxx, list);
		if (!dev_item)
			continue;
		if (dev_item->dev_index == aw87xxx->dev_index) {
			AW_DEV_LOGE(aw87xxx->dev,
				    "dev_index has already existing,check failed");
			return -EINVAL;
		}
	}

	return 0;
}

static int aw87xxx_dtsi_parse(struct aw87xxx *aw87xxx,
			      struct device_node *dev_node)
{
	int ret = 0;
	s32 dtsi_dev_index = -EINVAL;
	int rst_gpio = -1;

	ret = of_property_read_u32(dev_node, "dev_index", &dtsi_dev_index);
	if (ret < 0) {
		AW_DEV_LOGI(aw87xxx->dev,
			    "dev_index parse failed, user default[%d], ret=%d",
			    g_aw87xxx_dev_cnt, ret);
		aw87xxx->dev_index = g_aw87xxx_dev_cnt;
	} else {
		aw87xxx->dev_index = dtsi_dev_index;
		AW_DEV_LOGI(aw87xxx->dev, "parse dev_index=[%d]",
			    aw87xxx->dev_index);
	}

	ret = aw87xxx_dtsi_dev_index_check(aw87xxx);
	if (ret < 0)
		return ret;

	rst_gpio = of_get_named_gpio(dev_node, "reset-gpio", 0);
	if (rst_gpio < 0) {
		AW_DEV_LOGI(aw87xxx->dev,
			    "no reset gpio provided, hardware reset unavailable");
		aw87xxx->aw_dev.rst_gpio = -1;
		aw87xxx->aw_dev.hwen_status = AW_DEV_HWEN_INVALID;
	} else {
		aw87xxx->aw_dev.rst_gpio = rst_gpio;
		aw87xxx->aw_dev.hwen_status = AW_DEV_HWEN_OFF;
		AW_DEV_LOGI(aw87xxx->dev, "reset gpio[%d] parse succeed",
			    rst_gpio);
		if (gpio_is_valid(aw87xxx->aw_dev.rst_gpio)) {
			ret = devm_gpio_request_one(aw87xxx->dev,
						    aw87xxx->aw_dev.rst_gpio,
						    GPIOF_OUT_INIT_LOW,
						    "aw87xxx_reset");
			if (ret < 0) {
				AW_DEV_LOGE(aw87xxx->dev,
					    "reset request failed");
				return ret;
			}
		}
	}

	return 0;
}

static struct aw87xxx *aw87xxx_malloc_init(struct i2c_client *client)
{
	struct aw87xxx *aw87xxx = NULL;

	aw87xxx = devm_kzalloc(&client->dev, sizeof(struct aw87xxx), GFP_KERNEL);
	if (!aw87xxx) {
		AW_DEV_LOGE(&client->dev, "failed to devm_kzalloc aw87xxx");
		return NULL;
	}

	memset(aw87xxx, 0, sizeof(struct aw87xxx));
	aw87xxx->dev = &client->dev;
	aw87xxx->aw_dev.dev = &client->dev;
	aw87xxx->aw_dev.i2c = client;
	aw87xxx->aw_dev.i2c_bus = client->adapter->nr;
	aw87xxx->aw_dev.i2c_addr = (u8)client->addr;
	aw87xxx->aw_dev.hwen_status = AW_DEV_HWEN_INVALID;
	aw87xxx->aw_dev.reg_access = NULL;
	aw87xxx->fw_status = 0;
	aw87xxx->codec = NULL;
	aw87xxx->current_profile = aw87xxx->prof_off_name;

	mutex_init(&aw87xxx->reg_lock);

	AW_DEV_LOGI(&client->dev, "struct aw87xxx devm_kzalloc and init down");
	return aw87xxx;
}

static int aw87xxx_i2c_probe(struct i2c_client *client)
{
	struct device_node *dev_node = client->dev.of_node;
	struct aw87xxx *aw87xxx = NULL;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		AW_DEV_LOGE(&client->dev, "check_functionality failed");
		return -ENODEV;
	}

	aw87xxx = aw87xxx_malloc_init(client);
	if (!aw87xxx)
		return -1;

	i2c_set_clientdata(client, aw87xxx);

	ret = aw87xxx_dtsi_parse(aw87xxx, dev_node);
	if (ret < 0)
		goto exit_dev_init_failed;

	aw_dev_hw_pwr_ctrl(&aw87xxx->aw_dev, true);

	ret = aw_dev_init(&aw87xxx->aw_dev);
	if (ret < 0)
		goto exit_dev_init_failed;

	aw_dev_soft_reset(&aw87xxx->aw_dev);
	aw_dev_hw_pwr_ctrl(&aw87xxx->aw_dev, false);

	ret = sysfs_create_group(&aw87xxx->dev->kobj, &aw87xxx_attribute_group);
	if (ret < 0)
		AW_DEV_LOGE(aw87xxx->dev,
			    "failed to create sysfs nodes, will not allowed to use");

	aw87xxx_fw_load_init(aw87xxx);
	aw_monitor_init(aw87xxx->dev, &aw87xxx->monitor, dev_node);

	mutex_lock(&g_aw87xxx_mutex_lock);
	g_aw87xxx_dev_cnt++;
	list_add(&aw87xxx->list, &g_aw87xxx_list);
	mutex_unlock(&g_aw87xxx_mutex_lock);

	AW_DEV_LOGI(aw87xxx->dev, "succeed");
	return 0;

exit_dev_init_failed:
	AW_DEV_LOGE(aw87xxx->dev, "pa init failed");
	devm_kfree(&client->dev, aw87xxx);
	return ret;
}

static void aw87xxx_i2c_remove(struct i2c_client *client)
{
	struct aw87xxx *aw87xxx = i2c_get_clientdata(client);

	aw_monitor_exit(&aw87xxx->monitor);
	sysfs_remove_group(&aw87xxx->dev->kobj, &aw87xxx_attribute_group);
	aw_acf_profile_free(aw87xxx->dev, &aw87xxx->acf_info);
	aw_monitor_cfg_free(&aw87xxx->monitor);

	mutex_lock(&g_aw87xxx_mutex_lock);
	g_aw87xxx_dev_cnt--;
	list_del(&aw87xxx->list);
	mutex_unlock(&g_aw87xxx_mutex_lock);

	if (aw87xxx)
		devm_kfree(&client->dev, aw87xxx);
}

static void aw87xxx_i2c_shutdown(struct i2c_client *client)
{
	struct aw87xxx *aw87xxx = i2c_get_clientdata(client);

	AW_DEV_LOGI(&client->dev, "enter");
	if (aw87xxx)
		aw87xxx_update_profile(aw87xxx, aw87xxx->prof_off_name);
}

static const struct of_device_id extpa_of_match[] = {
	{ .compatible = "awinic,aw87xxx_pa" },
	{},
};

static const struct i2c_device_id aw87xxx_i2c_id[] = {
	{ AW87XXX_I2C_NAME, 0 },
	{},
};

static struct i2c_driver aw87xxx_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = AW87XXX_I2C_NAME,
		.of_match_table = extpa_of_match,
	},
	.probe = aw87xxx_i2c_probe,
	.remove = aw87xxx_i2c_remove,
	.shutdown = aw87xxx_i2c_shutdown,
	.id_table = aw87xxx_i2c_id,
};

static int __init aw87xxx_pa_init(void)
{
	int ret = 0;

	AW_LOGI("driver version: %s", AW87XXX_DRIVER_VERSION);

	ret = i2c_add_driver(&aw87xxx_i2c_driver);
	if (ret) {
		AW_LOGE("Unable to register driver (%d)", ret);
		return ret;
	}

	return 0;
}

static void __exit aw87xxx_pa_exit(void)
{
	AW_LOGI("enter");
	i2c_del_driver(&aw87xxx_i2c_driver);
}

module_init(aw87xxx_pa_init);
module_exit(aw87xxx_pa_exit);

MODULE_AUTHOR("<zhaozhongbo@awinic.com>");
MODULE_DESCRIPTION("awinic aw87xxx pa driver");
MODULE_LICENSE("GPL v2");
