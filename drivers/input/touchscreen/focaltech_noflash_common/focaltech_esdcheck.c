// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech ESD Check & LCD Error Recovery
 */
#include "focaltech_core.h"

#define ESDCHECK_WAIT_TIME		1000
#define LCD_ESD_PATCH			1

int tran_focaltech_lcd_need_reset;
EXPORT_SYMBOL(tran_focaltech_lcd_need_reset);

struct fts_esdcheck_st {
	u8 mode;
	u8 suspend;
	u8 proc_debug;
	u8 intr;
	u8 unused;
	u8 flow_work_hold_cnt;
	u8 flow_work_cnt_last;
	u32 hardware_reset_cnt;
	u32 nack_cnt;
	u32 dataerror_cnt;
};

static struct fts_esdcheck_st fts_esdcheck_data;

int idc_esdcheck_lcderror(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 val = 0;

	if (tran_focaltech_lcd_need_reset == 1)
		tran_focaltech_lcd_need_reset = 0;

	ret = fts_read_reg(FTS_REG_ESD_LCD_ERR, &val);
	if (ret < 0) {
		FTS_ERROR("read reg 0xED fail, ret=%d", ret);
		return ret;
	}

	if (val == 0xAA) {
		FTS_INFO("LCD ESD error detected (0xED=0xAA), need LCD reset");
		tran_focaltech_lcd_need_reset = 1;
		fts_release_all_finger();
		fts_tp_state_recovery(ts_data);
		return -EIO;
	}

	return 0;
}

static int get_chip_id(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int i = 0;
	u8 reg_value = 0;
	u8 chip_id = ts_data->ic_info.ids.chip_idh;

	for (i = 0; i < 3; i++) {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &reg_value);
		if (ret < 0) {
			FTS_ERROR("read chip id fail, ret=%d", ret);
			fts_esdcheck_data.nack_cnt++;
		} else {
			if ((reg_value == chip_id) || (fts_check_cid(ts_data, reg_value) == 0))
				return 0;
			FTS_DEBUG("read chip_id:%x,retry:%d", reg_value, i);
			fts_esdcheck_data.dataerror_cnt++;
		}
		msleep(10);
	}

	FTS_ERROR("get chip id fail after 3 retries");
	return -EIO;
}

static int get_flow_cnt(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 reg_value = 0;

	ret = fts_read_reg(FTS_REG_FLOW_WORK_CNT, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read reg0x91 fail, ret=%d", ret);
		fts_esdcheck_data.nack_cnt++;
		return ret;
	}

	if (reg_value == fts_esdcheck_data.flow_work_cnt_last) {
		fts_esdcheck_data.flow_work_hold_cnt++;
	} else {
		fts_esdcheck_data.flow_work_hold_cnt = 0;
		fts_esdcheck_data.flow_work_cnt_last = reg_value;
	}

	if (fts_esdcheck_data.flow_work_hold_cnt >= 5) {
		FTS_DEBUG("reg0x91 keep a value for 5 times, need execute TP reset");
		fts_esdcheck_data.flow_work_hold_cnt = 0;
		return -EIO;
	}

	return 0;
}

static void fts_esdcheck_tp_reset(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_data.hardware_reset_cnt++;
	fts_reset_proc(200);
	fts_tp_state_recovery(ts_data);
	FTS_FUNC_EXIT();
}

static int esdcheck_algorithm(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 reg_value = 0;
	bool hardware_reset = false;

	if (fts_esdcheck_data.suspend) {
		FTS_DEBUG("In suspend, not check esd");
		return 0;
	}

	if (fts_esdcheck_data.proc_debug) {
		FTS_DEBUG("In apk/adb command mode, not check esd");
		return 0;
	}

	ret = idc_esdcheck_lcderror(ts_data);
	if (ret < 0)
		return 0;

	ret = fts_read_reg(FTS_REG_WORKMODE, &reg_value);
	if (ret < 0) {
		fts_esdcheck_data.nack_cnt++;
	} else if ((reg_value & 0x70) == FTS_REG_WORKMODE_FACTORY_VALUE) {
		FTS_DEBUG("in factory mode(%x), no check esd", reg_value);
		return 0;
	}

	ret = get_chip_id(ts_data);
	if (ret < 0)
		hardware_reset = true;

	if (!hardware_reset) {
		ret = get_flow_cnt(ts_data);
		if (ret < 0)
			hardware_reset = true;
	}

	if (hardware_reset) {
		FTS_DEBUG("NoACK=%d, Error Data=%d, Hardware Reset=%d",
			  fts_esdcheck_data.nack_cnt,
			  fts_esdcheck_data.dataerror_cnt,
			  fts_esdcheck_data.hardware_reset_cnt);
		fts_esdcheck_tp_reset(ts_data);
	}

	return 0;
}

static void esdcheck_func(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
						   esdcheck_work.work);

	if (fts_esdcheck_data.mode) {
		esdcheck_algorithm(ts_data);
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
				   msecs_to_jiffies(ESDCHECK_WAIT_TIME));
	}
}

int fts_esdcheck_proc_busy(struct fts_ts_data *ts_data, bool proc_debug)
{
	fts_esdcheck_data.proc_debug = proc_debug;
	return 0;
}

int fts_esdcheck_switch(struct fts_ts_data *ts_data, bool enable)
{
	FTS_FUNC_ENTER();
	if (fts_esdcheck_data.mode) {
		if (enable) {
			FTS_DEBUG("ESD check start");
			fts_esdcheck_data.flow_work_hold_cnt = 0;
			fts_esdcheck_data.flow_work_cnt_last = 0;
			queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
					   msecs_to_jiffies(ESDCHECK_WAIT_TIME));
		} else {
			FTS_DEBUG("ESD check stop");
			cancel_delayed_work_sync(&ts_data->esdcheck_work);
		}
	}
	FTS_FUNC_EXIT();
	return 0;
}

int fts_esdcheck_suspend(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_switch(ts_data, false);
	fts_esdcheck_data.suspend = 1;
	FTS_FUNC_EXIT();
	return 0;
}

int fts_esdcheck_resume(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	fts_esdcheck_data.suspend = 0;
	fts_esdcheck_switch(ts_data, true);
	FTS_FUNC_EXIT();
	return 0;
}

int fts_esd_is_disable(void)
{
	return !fts_esdcheck_data.mode;
}

static ssize_t fts_esdcheck_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "Esd check: %s\n",
			 fts_esdcheck_data.mode ? "On" : "Off");
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_esdcheck_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable esdcheck");
		fts_esdcheck_data.mode = 1;
		fts_esdcheck_switch(ts_data, true);
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable esdcheck");
		fts_esdcheck_switch(ts_data, false);
		fts_esdcheck_data.mode = 0;
	}
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static DEVICE_ATTR(fts_esd_mode, 0644, fts_esdcheck_show, fts_esdcheck_store);

static struct attribute *fts_esd_mode_attrs[] = {
	&dev_attr_fts_esd_mode.attr,
	NULL,
};

static struct attribute_group fts_esd_group = {
	.attrs = fts_esd_mode_attrs,
};

int fts_esdcheck_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (ts_data->ts_workqueue) {
		INIT_DELAYED_WORK(&ts_data->esdcheck_work, esdcheck_func);
	} else {
		FTS_ERROR("fts workqueue is NULL, can't run esd check function");
		return -EINVAL;
	}

	memset(&fts_esdcheck_data, 0, sizeof(struct fts_esdcheck_st));
	fts_esdcheck_data.mode = 1;
	fts_esdcheck_switch(ts_data, true);

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_esd_group);
	if (ret) {
		FTS_ERROR("create esd sysfs fail");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_esd_group);
	}

	FTS_FUNC_EXIT();
	return 0;
}

int fts_esdcheck_exit(struct fts_ts_data *ts_data)
{
	cancel_delayed_work_sync(&ts_data->esdcheck_work);
	sysfs_remove_group(&ts_data->dev->kobj, &fts_esd_group);
	return 0;
}
