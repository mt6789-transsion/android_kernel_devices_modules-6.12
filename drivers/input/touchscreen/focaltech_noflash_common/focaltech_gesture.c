// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech Gesture Support (integrated with tran_touch)
 */
#include "focaltech_core.h"

#define FTS_GESTURE_POINTS_MAX		6
#define FTS_GESTURE_DATA_LEN		(FTS_GESTURE_POINTS_MAX * 4 + 2)

#define KEY_GESTURE_U			KEY_U
#define KEY_GESTURE_UP			KEY_UP
#define KEY_GESTURE_DOWN		KEY_DOWN
#define KEY_GESTURE_LEFT		KEY_LEFT
#define KEY_GESTURE_RIGHT		KEY_RIGHT
#define KEY_GESTURE_O			KEY_O
#define KEY_GESTURE_E			KEY_E
#define KEY_GESTURE_M			KEY_M
#define KEY_GESTURE_L			KEY_L
#define KEY_GESTURE_W			KEY_W
#define KEY_GESTURE_S			KEY_S
#define KEY_GESTURE_V			KEY_V
#define KEY_GESTURE_C			KEY_C
#define KEY_GESTURE_Z			KEY_Z

struct fts_gesture_st {
	u8 gesture_id;
	u8 point_num;
	u16 coordinate_x[FTS_GESTURE_POINTS_MAX];
	u16 coordinate_y[FTS_GESTURE_POINTS_MAX];
};

static struct fts_gesture_st fts_gesture_data;

static ssize_t fts_gesture_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	fts_read_reg(FTS_REG_GESTURE_EN, &val);
	count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
			 ts_data->gesture_support ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable gesture");
		ts_data->gesture_support = true;
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable gesture");
		ts_data->gesture_support = false;
	}
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_buf_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int count = 0;
	int i = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct fts_gesture_st *gesture = &fts_gesture_data;

	mutex_lock(&ts_data->input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "Gesture ID:%d\n", gesture->gesture_id);
	count += snprintf(buf + count, PAGE_SIZE, "Gesture PointNum:%d\n",
			  gesture->point_num);
	count += snprintf(buf + count, PAGE_SIZE, "Gesture Points Buffer:\n");
	for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%3d(%4d,%4d) ",
				  i, gesture->coordinate_x[i],
				  gesture->coordinate_y[i]);
		if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, PAGE_SIZE, "\n");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_buf_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_gesture_bm_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "gesture bmode:%d\n", ts_data->gesture_bmode);
}

static ssize_t fts_gesture_bm_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int value = 0xFF;
	int ret = 0;

	ret = sscanf(buf, "%d", &value);
	if (ret == 1) {
		FTS_DEBUG("gesture bmode change %d->%d", ts_data->gesture_bmode, value);
		ts_data->gesture_bmode = value;
	}

	return count;
}

static DEVICE_ATTR(fts_gesture_mode, 0644, fts_gesture_show, fts_gesture_store);
static DEVICE_ATTR(fts_gesture_buf, 0644, fts_gesture_buf_show, fts_gesture_buf_store);
static DEVICE_ATTR(fts_gesture_bm, 0644, fts_gesture_bm_show, fts_gesture_bm_store);

static struct attribute *fts_gesture_mode_attrs[] = {
	&dev_attr_fts_gesture_mode.attr,
	&dev_attr_fts_gesture_buf.attr,
	&dev_attr_fts_gesture_bm.attr,
	NULL,
};

static struct attribute_group fts_gesture_group = {
	.attrs = fts_gesture_mode_attrs,
};

int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data)
{
	int ret = 0;
	int i = 0;
	int index = 0;
	u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
	u8 cmd = FTS_REG_GESTURE_OUTPUT_ADDRESS;
	struct fts_gesture_st *gesture = &fts_gesture_data;
	struct tran_tpd_ops *ops = NULL;

	if (!ts_data->gesture_support) {
		FTS_ERROR("gesture not support");
		return -EINVAL;
	}

	ret = fts_read(&cmd, 1, buf, FTS_GESTURE_DATA_LEN);
	if (ret < 0) {
		FTS_ERROR("read gesture header data fail");
		return ret;
	}

	gesture->gesture_id = buf[0];
	gesture->point_num = buf[1];
	FTS_DEBUG("gesture_id=%d, point_num=%d", gesture->gesture_id, gesture->point_num);

	for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
		index = 4 * i + 2;
		gesture->coordinate_x[i] = (u16)(((buf[index] & 0x0F) << 8) + buf[index + 1]);
		gesture->coordinate_y[i] = (u16)(((buf[index + 2] & 0x0F) << 8) + buf[index + 3]);
	}

	if (get_tpd_ops(&fts_noflash_controller, &ops) && ops->gesture_id_translate) {
		FTS_INFO("gesture id:0x%x", gesture->gesture_id);
		ret = ops->gesture_id_translate(gesture->gesture_id);
		if (ret < 0)
			FTS_ERROR("gesture_id_translate fail");
	}

	return 0;
}

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->gesture_support && ts_data->suspended) {
		FTS_DEBUG("gesture recovery...");
		fts_write_reg(0xD1, 0xFF);
		fts_write_reg(0xD2, 0xFF);
		fts_write_reg(0xD5, 0xFF);
		fts_write_reg(0xD6, 0xFF);
		fts_write_reg(0xD7, 0xFF);
		fts_write_reg(0xD8, 0xFF);
		fts_write_reg(FTS_REG_GESTURE_EN, 0x01);
	}
}

int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
	int i = 0;
	u8 state = 0xFF;

	FTS_FUNC_ENTER();
	if (enable_irq_wake(ts_data->irq))
		FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);

	for (i = 0; i < 5; i++) {
		fts_write_reg(0xD1, 0xFF);
		fts_write_reg(0xD2, 0xFF);
		fts_write_reg(0xD5, 0xFF);
		fts_write_reg(0xD6, 0xFF);
		fts_write_reg(0xD7, 0xFF);
		fts_write_reg(0xD8, 0xFF);
		fts_write_reg(FTS_REG_GESTURE_EN, 0x01);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == 0x01)
			break;
	}

	if (i >= 5)
		FTS_ERROR("make IC enter into gesture(suspend) fail,state:%x", state);
	else
		FTS_INFO("Enter into gesture(suspend) successfully");

	FTS_FUNC_EXIT();
	return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
	int i = 0;
	u8 state = 0xFF;

	FTS_FUNC_ENTER();
	if (disable_irq_wake(ts_data->irq))
		FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);

	for (i = 0; i < 5; i++) {
		fts_write_reg(FTS_REG_GESTURE_EN, 0x00);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == 0x00)
			break;
	}

	if (i >= 5)
		FTS_ERROR("make IC exit gesture(resume) fail,state:%x", state);
	else
		FTS_INFO("resume from gesture successfully");

	FTS_FUNC_EXIT();
	return 0;
}

int fts_gesture_init(struct fts_ts_data *ts_data)
{
	struct input_dev *input_dev = fts_noflash_controller.input_dev;
	int ret = 0;

	FTS_FUNC_ENTER();
	input_set_capability(input_dev, EV_KEY, KEY_F13);
	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_gesture_group);
	if (ret) {
		FTS_ERROR("create gesture sysfs fail");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
	}

	memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
	ts_data->gesture_bmode = 1;
	ts_data->gesture_support = true;

	FTS_FUNC_EXIT();
	return 0;
}

int fts_gesture_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
	FTS_FUNC_EXIT();
	return 0;
}
