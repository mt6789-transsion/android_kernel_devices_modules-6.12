/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include "aw87xxx.h"
#include "aw87xxx_dsp.h"
#include "aw87xxx_monitor.h"
#include "aw87xxx_log.h"

#define AW_ESD_RECOVER_MAX_CNT		10

void aw_monitor_cfg_free(struct aw_monitor *monitor)
{
	struct aw87xxx *aw87xxx = container_of(monitor, struct aw87xxx, monitor);

	monitor->monitor_cfg.bin_status = AW_MONITOR_CFG_WAIT;
	memset(&monitor->monitor_cfg.header, 0,
	       sizeof(struct aw_monitor_header));

	if (monitor->monitor_cfg.vmax_cfg) {
		devm_kfree(aw87xxx->dev, monitor->monitor_cfg.vmax_cfg);
		monitor->monitor_cfg.vmax_cfg = NULL;
	}
}

static int aw_monitor_bin_check_sum(struct device *dev, const char *data,
				    u32 data_len)
{
	u32 i = 0;
	u32 data_sum = 0;
	u32 check_sum = 0;

	if (data_len >= 4) {
		for (i = 4; i < data_len; i++)
			data_sum += (u8)data[i];
	}

	check_sum = *(const u32 *)data;
	if (check_sum != data_sum) {
		AW_DEV_LOGE(dev,
			    "check_sum[%d] is not equal to data_sum[%d]",
			    check_sum, data_sum);
		return -ENOMEM;
	}

	return 0;
}

static int aw_monitor_check_header_v_1_0_0(struct device *dev, const char *data,
					   u32 data_len)
{
	u32 bin_data_type = *(const u32 *)(data + 8);
	u32 bin_data_ver = *(const u32 *)(data + 12);
	u32 data_size = *(const u32 *)(data + 16);
	u32 data_byte_len = *(const u32 *)(data + 36);

	if (bin_data_type != AW_MONITOR_BIN_DATA_TYPE) {
		AW_DEV_LOGE(dev, "monitor data_type check error!");
		return -EINVAL;
	}

	if (data_size != AW_MONITOR_BIN_DATA_SIZE) {
		AW_DEV_LOGE(dev, "monitor data_size error!");
		return -EINVAL;
	}

	if (data_byte_len != AW_MONITOR_BIN_DATA_BYTE_LEN) {
		AW_DEV_LOGE(dev, "monitor data_byte_len error!");
		return -EINVAL;
	}

	if (bin_data_ver != AW_MONITOR_BIN_DATA_VER_V1) {
		AW_DEV_LOGE(dev, "bin data_ver[0x%x] non support", bin_data_ver);
		return -EINVAL;
	}

	return 0;
}

static int aw_monitor_check_data_v1_size(struct device *dev, const char *data,
					 u32 data_len)
{
	const struct aw_monitor_header *hdr = NULL;
	u32 expected_len = 0;

	if (data_len < AW_MONITOR_BIN_HDR_LEN + sizeof(struct aw_monitor_header)) {
		AW_DEV_LOGE(dev,
			    "bin len is less than aw_bin_header and monitoor_header,check failed");
		return -EINVAL;
	}

	hdr = (const struct aw_monitor_header *)(data + AW_MONITOR_BIN_HDR_LEN);
	expected_len = hdr->step_count * sizeof(struct vmax_step_config);
	if (data_len - (AW_MONITOR_BIN_HDR_LEN + sizeof(struct aw_monitor_header)) <
	    expected_len) {
		AW_DEV_LOGE(dev, "bin data len is not enough,check failed");
		return -EINVAL;
	}

	return 0;
}

static int aw_monitor_bin_check(struct device *dev, const char *data,
				u32 data_len)
{
	u32 header_ver = 0;
	int ret = 0;

	if (!data || data_len == 0) {
		AW_DEV_LOGE(dev, "none data to parse");
		return -EINVAL;
	}

	ret = aw_monitor_bin_check_sum(dev, data, data_len);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "bin data check sum failed");
		return ret;
	}

	if (data_len < AW_MONITOR_BIN_HDR_LEN) {
		AW_DEV_LOGE(dev,
			    "bin len is less than aw_bin_header,check failed");
		return -EINVAL;
	}

	header_ver = *(const u32 *)(data + 4);
	if (header_ver != AW_MONITOR_BIN_HDR_VER) {
		AW_DEV_LOGE(dev, "bin version[0x%x] non support", header_ver);
		return -EINVAL;
	}

	ret = aw_monitor_check_header_v_1_0_0(dev, data, data_len);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "monitor bin haeder info check error!");
		return ret;
	}

	ret = aw_monitor_check_data_v1_size(dev, data, data_len);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "bin header info check failed");
		return ret;
	}

	return 0;
}

static int aw_monitor_parse_data_v1(struct device *dev,
				    struct aw_monitor *monitor,
				    const char *data)
{
	struct aw_monitor_cfg *cfg = &monitor->monitor_cfg;
	const struct aw_monitor_header *hdr =
		(const struct aw_monitor_header *)(data + AW_MONITOR_BIN_HDR_LEN);
	const u8 *step_data =
		(const u8 *)(data + AW_MONITOR_BIN_HDR_LEN +
			     sizeof(struct aw_monitor_header));
	struct vmax_step_config *vmax_cfg = NULL;
	u32 i = 0;

	memcpy(&cfg->header, hdr, sizeof(struct aw_monitor_header));
	AW_DEV_LOGI(dev,
		    "monitor_switch:%d, monitor_time:%d (ms), monitor_count:%d, step_count:%d",
		    cfg->header.monitor_switch, cfg->header.monitor_time,
		    cfg->header.monitor_count, cfg->header.step_count);

	if (cfg->header.step_count > 0) {
		vmax_cfg = devm_kzalloc(dev,
					cfg->header.step_count *
					sizeof(struct vmax_step_config),
					GFP_KERNEL);
		if (!vmax_cfg) {
			AW_DEV_LOGE(dev, "vmax_cfg vmalloc failed");
			AW_DEV_LOGE(dev, "vmax_data parse failed");
			aw_monitor_cfg_free(monitor);
			return -ENOMEM;
		}

		for (i = 0; i < cfg->header.step_count; i++) {
			const u8 *entry = step_data + i * 12;

			vmax_cfg[i].vbat_min = *(const u32 *)(entry + 0);
			vmax_cfg[i].vbat_max = *(const u32 *)(entry + 4);
			vmax_cfg[i].vmax_vol =
				(int)(((u32)entry[11] << 24) |
				      ((u32)entry[10] << 16) |
				      ((u32)entry[9] << 8) |
				      (u32)entry[8]);
		}

		for (i = 0; i < cfg->header.step_count; i++) {
			AW_DEV_LOGI(dev,
				    "vbat_min:%d, vbat_max%d, vmax_vol:0x%x",
				    vmax_cfg[i].vbat_min, vmax_cfg[i].vbat_max,
				    vmax_cfg[i].vmax_vol);
		}
	}

	cfg->vmax_cfg = vmax_cfg;
	AW_DEV_LOGI(dev, "vmax_data parse succeed");
	cfg->bin_status = AW_MONITOR_CFG_OK;

	return 0;
}

int aw_monitor_bin_parse(struct device *dev, const char *monitor_data,
			 u32 data_len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	struct aw_monitor *monitor = NULL;
	int ret = 0;

	if (!aw87xxx) {
		AW_DEV_LOGE(dev, "get struct aw87xxx failed");
		return -EINVAL;
	}

	monitor = &aw87xxx->monitor;
	monitor->monitor_cfg.bin_status = AW_MONITOR_CFG_WAIT;

	AW_DEV_LOGI(dev, "monitor bin parse version: %s",
		    AW_MONITOR_BIN_PARSE_VER);

	ret = aw_monitor_bin_check(dev, monitor_data, data_len);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "monitor bin check failed");
		return ret;
	}

	return aw_monitor_parse_data_v1(dev, monitor, monitor_data);
}

static int aw_monitor_get_battery_capacity(struct device *dev, int *vbat_capacity)
{
	char name[] = "battery";
	struct power_supply *psy = NULL;
	union power_supply_propval prop = { 0 };
	int ret = 0;

	psy = power_supply_get_by_name(name);
	if (!psy) {
		AW_DEV_LOGE(dev, "no struct power supply name:%s", name);
		return -EINVAL;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "get vbat capacity failed");
		return -EINVAL;
	}

	*vbat_capacity = prop.intval;
	AW_DEV_LOGI(dev, "The percentage is %d", *vbat_capacity);
	return 0;
}

static int aw_search_vmax_from_table(struct device *dev,
				     struct aw_monitor *monitor,
				     const int vbat_vol, int *vmax_vol)
{
	struct aw_monitor_cfg *cfg = &monitor->monitor_cfg;
	u32 i = 0;

	if (cfg->bin_status == AW_MONITOR_CFG_WAIT) {
		AW_DEV_LOGE(dev, "vmax_cfg not loaded or parse failed");
		return -EINVAL;
	}

	if (cfg->header.step_count == 0) {
		AW_DEV_LOGE(dev, "vmax_cfg not found");
		return -EINVAL;
	}

	if (vbat_vol == 100) {
		*vmax_vol = 0;
		return 0;
	}

	for (i = 0; i < cfg->header.step_count; i++) {
		if ((u32)vbat_vol >= cfg->vmax_cfg[i].vbat_min &&
		    (u32)vbat_vol < cfg->vmax_cfg[i].vbat_max) {
			*vmax_vol = cfg->vmax_cfg[i].vmax_vol;
			return 0;
		}
	}

	AW_DEV_LOGE(dev, "vmax_cfg not found");
	return -EINVAL;
}

void aw_monitor_stop(struct aw_monitor *monitor)
{
	cancel_delayed_work_sync(&monitor->with_dsp_work);
}

void aw_monitor_start(struct aw_monitor *monitor)
{
	struct aw87xxx *aw87xxx = container_of(monitor, struct aw87xxx, monitor);
	struct aw_monitor_cfg *cfg = &monitor->monitor_cfg;
	int ret = 0;

	ret = aw_dev_check_reg_is_rec_mode(&aw87xxx->aw_dev);
	if (ret < 0)
		AW_DEV_LOGE(aw87xxx->dev, "get reg current mode failed");

	if ((monitor->esd_enable == AW_MONITOR_ESD_ENABLE) ||
	    (cfg->header.monitor_switch &&
	     !aw87xxx->aw_dev.is_rec_mode &&
	     monitor->open_dsp_en &&
	     cfg->bin_status == AW_MONITOR_CFG_OK)) {
		monitor->pre_vmax = AW_MONITOR_VMAX_NONE;
		monitor->first_entry = AW_FIRST_ENTRY;
		monitor->timer_cnt = 0;
		monitor->vbat_sum = 0;
		schedule_delayed_work(&monitor->with_dsp_work,
				      msecs_to_jiffies(cfg->header.monitor_time));
	}
}

int aw_monitor_no_dsp_get_vmax(struct aw_monitor *monitor, int *vmax)
{
	struct aw87xxx *aw87xxx = container_of(monitor, struct aw87xxx, monitor);
	int vbat_capacity = 0;
	int vmax_vol = 0;
	int ret = 0;

	if (!monitor->monitor_cfg.header.monitor_switch) {
		AW_DEV_LOGI(aw87xxx->dev, "monitor switch is off");
		return -EINVAL;
	}

	if (aw87xxx->aw_dev.is_rec_mode) {
		AW_DEV_LOGI(aw87xxx->dev, "receiver mode, not adjust vmax");
		return -EINVAL;
	}

	if (monitor->custom_capacity) {
		vbat_capacity = monitor->custom_capacity;
	} else {
		ret = aw_monitor_get_battery_capacity(aw87xxx->dev,
						      &vbat_capacity);
		if (ret < 0)
			return ret;
	}

	ret = aw_search_vmax_from_table(aw87xxx->dev, monitor,
					vbat_capacity, &vmax_vol);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "not find vmax_vol");
		return ret;
	}

	*vmax = vmax_vol;
	return 0;
}

static int aw_chip_status_recover(struct aw87xxx *aw87xxx)
{
	struct aw_monitor *monitor = &aw87xxx->monitor;
	char *profile = aw87xxx->current_profile;
	int ret = 0;

	ret = aw87xxx_esd_update_profile(aw87xxx, profile);
	if (ret < 0) {
		AW_DEV_LOGE(aw87xxx->dev, "load profile[%s] failed ", profile);
		return ret;
	}

	AW_DEV_LOGI(aw87xxx->dev, "current prof[%s], dev_index[%d] ",
		    profile, aw87xxx->dev_index);
	monitor->pre_vmax = AW_MONITOR_VMAX_NONE;
	monitor->first_entry = AW_FIRST_ENTRY;
	monitor->timer_cnt = 0;
	monitor->vbat_sum = 0;

	return 0;
}

static int aw_monitor_chip_esd_check_work(struct aw87xxx *aw87xxx)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < AW_ESD_RECOVER_MAX_CNT; i++) {
		ret = aw_dev_esd_reg_status_check(&aw87xxx->aw_dev);
		if (ret < 0) {
			aw_chip_status_recover(aw87xxx);
			msleep(1);
		} else {
			return 0;
		}
	}

	AW_DEV_LOGE(aw87xxx->dev, "chip status recover failed,chip off");
	aw87xxx_esd_update_profile(aw87xxx, aw87xxx->prof_off_name);
	return -EINVAL;
}

static void aw_monitor_update_vmax_to_dsp(struct device *dev,
					  struct aw_monitor *monitor,
					  int vmax_set)
{
	int ret = 0;
	int enable = 0;

	if (monitor->pre_vmax == (u32)vmax_set) {
		AW_DEV_LOGI(dev, "vmax=0x%x no change", vmax_set);
		return;
	}

	ret = aw_dsp_get_rx_module_enable(&enable);
	if (ret < 0 || !enable) {
		AW_DEV_LOGE(dev,
			    "get rx failed or rx disable, ret=%d, enable=%d",
			    ret, enable);
		return;
	}

	ret = aw_dsp_set_vmax((u32)vmax_set, monitor->dev_index);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "set vmax failed");
		return;
	}

	monitor->pre_vmax = (u32)vmax_set;
	AW_DEV_LOGI(dev, "set vmax=0x%x succeed", vmax_set);
}

static void aw_monitor_with_dsp_vmax_work(struct device *dev,
					  struct aw_monitor *monitor)
{
	int ret = 0;
	int vbat_capacity = 0;
	int ave_capacity = 0;
	int vmax_set = 0;

	ret = aw_monitor_get_battery_capacity(dev, &vbat_capacity);
	if (ret < 0)
		return;

	if (monitor->timer_cnt < monitor->monitor_cfg.header.monitor_count) {
		monitor->timer_cnt++;
		monitor->vbat_sum += vbat_capacity;
		AW_DEV_LOGI(dev, "timer_cnt = %d", monitor->timer_cnt);
	}

	if (monitor->timer_cnt >= monitor->monitor_cfg.header.monitor_count ||
	    monitor->first_entry == AW_FIRST_ENTRY) {
		if (monitor->first_entry == AW_FIRST_ENTRY)
			monitor->first_entry = AW_NOT_FIRST_ENTRY;

		ave_capacity = monitor->vbat_sum / monitor->timer_cnt;
		if (monitor->custom_capacity)
			ave_capacity = monitor->custom_capacity;

		AW_DEV_LOGI(dev, "get average capacity = %d", ave_capacity);

		ret = aw_search_vmax_from_table(dev, monitor,
						ave_capacity, &vmax_set);
		if (ret < 0) {
			AW_DEV_LOGE(dev, "not find vmax_vol");
		} else {
			aw_monitor_update_vmax_to_dsp(dev, monitor, vmax_set);
		}

		monitor->timer_cnt = 0;
		monitor->vbat_sum = 0;
	}
}

static void aw_monitor_work_func(struct work_struct *work)
{
	struct aw87xxx *aw87xxx = container_of(work, struct aw87xxx,
					      monitor.with_dsp_work.work);
	struct aw_monitor *monitor = &aw87xxx->monitor;
	struct aw_monitor_cfg *cfg = &monitor->monitor_cfg;
	int ret = 0;

	if (monitor->esd_enable == AW_MONITOR_ESD_ENABLE) {
		ret = aw_monitor_chip_esd_check_work(aw87xxx);
		if (ret < 0)
			return;
	}

	if (cfg->header.monitor_switch &&
	    !aw87xxx->aw_dev.is_rec_mode &&
	    monitor->open_dsp_en &&
	    cfg->bin_status == AW_MONITOR_CFG_OK) {
		aw_monitor_with_dsp_vmax_work(aw87xxx->dev, monitor);
	}

	if ((monitor->esd_enable == AW_MONITOR_ESD_ENABLE) ||
	    (cfg->header.monitor_switch &&
	     !aw87xxx->aw_dev.is_rec_mode &&
	     monitor->open_dsp_en &&
	     cfg->bin_status == AW_MONITOR_CFG_OK)) {
		schedule_delayed_work(&monitor->with_dsp_work,
				      msecs_to_jiffies(cfg->header.monitor_time));
	}
}

/* Sysfs attributes: monitor DSP control group */
static ssize_t aw_attr_get_rx(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int enable = 0;
	ssize_t len = 0;

	ret = aw_dsp_get_rx_module_enable(&enable);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "read rx failed");
		return ret;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "rx: %d\n", enable);
	return len;
}

static ssize_t aw_attr_set_rx(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	int ret = 0;
	u32 enable = 0;

	ret = kstrtouint(buf, 0, &enable);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set rx=%d", enable);
	ret = aw_dsp_set_rx_module_enable(enable);
	if (ret < 0)
		AW_DEV_LOGE(dev, "set rx failed");

	return len;
}

static ssize_t aw_attr_get_monitor_count(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "monitor_count: %d\n",
			aw87xxx->monitor.monitor_cfg.header.monitor_count);
	return len;
}

static ssize_t aw_attr_set_monitor_count(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	u32 count = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &count);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set monitor_count=%d", count);
	aw87xxx->monitor.monitor_cfg.header.monitor_count = count;
	return len;
}

static ssize_t aw_attr_get_monitor_time(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "monitor_time: %d (ms)\n",
			aw87xxx->monitor.monitor_cfg.header.monitor_time);
	return len;
}

static ssize_t aw_attr_set_monitor_time(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	u32 time = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &time);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set monitor_time=%d", time);
	aw87xxx->monitor.monitor_cfg.header.monitor_time = time;
	return len;
}

static ssize_t aw_attr_get_monitor_switch(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "monitor_switch: %d\n",
			aw87xxx->monitor.monitor_cfg.header.monitor_switch);
	return len;
}

static ssize_t aw_attr_set_monitor_switch(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	u32 enable = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &enable);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set monitor_switch=%d", enable);
	aw87xxx->monitor.monitor_cfg.header.monitor_switch = enable ? 1 : 0;
	return len;
}

/* Sysfs attributes: monitor vol_adjust group */
static ssize_t aw_attr_get_vmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	struct aw_monitor *monitor = &aw87xxx->monitor;
	ssize_t len = 0;
	int ret = 0;
	u32 vmax_get = 0;
	int no_dsp_vmax = 0;

	if (monitor->open_dsp_en) {
		ret = aw_dsp_get_vmax(&vmax_get, monitor->dev_index);
		if (ret < 0) {
			AW_DEV_LOGE(dev, "get dsp vmax fail, ret=%d", ret);
			return ret;
		}
		len += snprintf(buf + len, PAGE_SIZE - len,
				"get vmax: 0x%x\n", vmax_get);
	} else {
		ret = aw_monitor_no_dsp_get_vmax(monitor, &no_dsp_vmax);
		if (ret < 0) {
			AW_DEV_LOGE(dev, "get vmax fail, ret=%d", ret);
			return ret;
		}
		len += snprintf(buf + len, PAGE_SIZE - len,
				"get vmax: 0x%x\n", no_dsp_vmax);
	}

	return len;
}

static ssize_t aw_attr_set_vmax(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	struct aw_monitor *monitor = &aw87xxx->monitor;
	u32 vmax_set = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &vmax_set);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set vmax=0x%x", vmax_set);
	if (monitor->open_dsp_en) {
		ret = aw_dsp_set_vmax(vmax_set, monitor->dev_index);
		if (ret < 0)
			AW_DEV_LOGE(dev, "set dsp vmax failed");
	} else {
		AW_DEV_LOGE(dev, "dsp is not enabled");
	}

	return len;
}

static ssize_t aw_attr_get_vbat(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "vbat capacity: %d\n",
			aw87xxx->monitor.custom_capacity);
	return len;
}

static ssize_t aw_attr_set_vbat(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	u32 capacity = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &capacity);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set custom_capacity=%d", capacity);
	aw87xxx->monitor.custom_capacity = capacity;
	return len;
}

static ssize_t aw_attr_get_esd_enable(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "esd_enable: %d\n",
			aw87xxx->monitor.esd_enable);
	return len;
}

static ssize_t aw_attr_set_esd_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	u32 enable = 0;
	int ret = 0;

	ret = kstrtouint(buf, 0, &enable);
	if (ret < 0) {
		AW_DEV_LOGE(dev, "input value error");
		return len;
	}

	AW_DEV_LOGI(dev, "set esd_enable=%d", enable);
	aw87xxx->monitor.esd_enable = enable ? AW_MONITOR_ESD_ENABLE :
					       AW_MONITOR_ESD_DISABLE;
	return len;
}

static DEVICE_ATTR(rx, 0644, aw_attr_get_rx, aw_attr_set_rx);
static DEVICE_ATTR(monitor_count, 0644,
		   aw_attr_get_monitor_count, aw_attr_set_monitor_count);
static DEVICE_ATTR(monitor_time, 0644,
		   aw_attr_get_monitor_time, aw_attr_set_monitor_time);
static DEVICE_ATTR(monitor_switch, 0644,
		   aw_attr_get_monitor_switch, aw_attr_set_monitor_switch);

static struct attribute *aw_monitor_control[] = {
	&dev_attr_monitor_switch.attr,
	&dev_attr_monitor_time.attr,
	&dev_attr_monitor_count.attr,
	&dev_attr_rx.attr,
	NULL,
};

static struct attribute_group aw_monitor_control_group = {
	.attrs = aw_monitor_control,
};

static DEVICE_ATTR(esd_enable, 0644,
		   aw_attr_get_esd_enable, aw_attr_set_esd_enable);
static DEVICE_ATTR(vbat, 0644, aw_attr_get_vbat, aw_attr_set_vbat);
static DEVICE_ATTR(vmax, 0644, aw_attr_get_vmax, aw_attr_set_vmax);

static struct attribute *aw_monitor_vol_adjust[] = {
	&dev_attr_esd_enable.attr,
	&dev_attr_vbat.attr,
	&dev_attr_vmax.attr,
	NULL,
};

static struct attribute_group aw_monitor_vol_adjust_group = {
	.attrs = aw_monitor_vol_adjust,
};

static void aw_monitor_dtsi_parse(struct device *dev,
				  struct aw_monitor *monitor,
				  struct device_node *dev_node)
{
	const char *esd_enable_str = NULL;
	int ret = 0;

	ret = of_property_read_string(dev_node, "esd-enable", &esd_enable_str);
	if (ret < 0) {
		AW_DEV_LOGI(dev,
			    "esd_enable parse failed, user default[disable]");
		monitor->esd_enable = AW_MONITOR_ESD_DISABLE;
	} else {
		if (!strcmp(esd_enable_str, "true"))
			monitor->esd_enable = AW_MONITOR_ESD_ENABLE;
		else
			monitor->esd_enable = AW_MONITOR_ESD_DISABLE;

		AW_DEV_LOGI(dev, "parse esd-enable=[%s]",
			    monitor->esd_enable ? "true" : "false");
	}
}

void aw_monitor_init(struct device *dev, struct aw_monitor *monitor,
		     struct device_node *dev_node)
{
	struct aw87xxx *aw87xxx = container_of(monitor, struct aw87xxx, monitor);
	int ret = 0;

	monitor->dev_index = aw87xxx->dev_index;
	monitor->monitor_cfg.header.monitor_time = AW_MONITOR_DEFAULT_TIME_MS;

	aw_monitor_dtsi_parse(dev, monitor, dev_node);

	monitor->open_dsp_en = aw_dsp_isEnable() ? 1 : 0;

	ret = sysfs_create_group(&dev->kobj, &aw_monitor_vol_adjust_group);
	if (ret < 0)
		AW_DEV_LOGE(dev, "failed to create monitor vol_adjust sysfs nodes");

	INIT_DELAYED_WORK(&monitor->with_dsp_work, aw_monitor_work_func);

	if (monitor->open_dsp_en) {
		ret = sysfs_create_group(&dev->kobj, &aw_monitor_control_group);
		if (ret < 0)
			AW_DEV_LOGE(dev,
				    "failed to create monitor dsp control sysfs nodes");
	}

	if (ret == 0)
		AW_DEV_LOGI(dev, "monitor init succeed");
}

void aw_monitor_exit(struct aw_monitor *monitor)
{
	struct aw87xxx *aw87xxx = container_of(monitor, struct aw87xxx, monitor);

	sysfs_remove_group(&aw87xxx->dev->kobj, &aw_monitor_vol_adjust_group);
	aw_monitor_stop(monitor);
	if (monitor->open_dsp_en)
		sysfs_remove_group(&aw87xxx->dev->kobj,
				   &aw_monitor_control_group);
}
