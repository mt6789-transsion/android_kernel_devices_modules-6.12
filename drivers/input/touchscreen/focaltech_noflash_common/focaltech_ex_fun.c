// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech APK Debug Procfs, Edge Reject, and Sysfs Interface
 */
#include "focaltech_core.h"

#define PROC_NAME			"ftxxxx-debug"
#define PROC_TA_NAME			"fts_ta"

#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER		2
#define PROC_AUTOCLB			4
#define PROC_UPGRADE_INFO		5
#define PROC_WRITE_DATA			6
#define PROC_READ_DATA			7
#define PROC_SET_TEST_FLAG		8
#define PROC_SET_SLAVE_ADDR		10
#define PROC_HW_RESET			11
#define PROC_READ_STATUS		12
#define PROC_SET_BOOT_MODE		13
#define PROC_ENTER_TEST_ENVIRONMENT	14
#define PROC_WRITE_DATA_DIRECT		16
#define PROC_READ_DATA_DIRECT		17
#define PROC_CONFIGURE			18
#define PROC_CONFIGURE_INTR		20
#define PROC_NAME_LEN			128

static struct {
	int type;
	int reg;
	int val;
	int res;
	int opbuf_len;
	u8 *opbuf;
} rw_op;

static u8 proc_opmode;
static u8 proc_cmd[2];

static int fts_ta_open(struct inode *inode, struct file *file)
{
	struct fts_ts_data *ts_data = pde_data(inode);

	if (ts_data->ta_buf) {
		FTS_INFO("TP is busy, please try again");
		return -EBUSY;
	}

	ts_data->ta_buf = kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
	if (!ts_data->ta_buf) {
		FTS_ERROR("kzalloc for ta_buf fails");
		return -ENOMEM;
	}

	ts_data->ta_flag = 0;
	ts_data->ta_mode = true;
	return 0;
}

static int fts_ta_release(struct inode *inode, struct file *file)
{
	struct fts_ts_data *ts_data = pde_data(inode);

	ts_data->ta_mode = false;
	kfree(ts_data->ta_buf);
	ts_data->ta_buf = NULL;
	return 0;
}

static ssize_t fts_ta_read(struct file *filp, char __user *buff,
			   size_t count, loff_t *ppos)
{
	int ret = 0;
	struct fts_ts_data *ts_data = pde_data(file_inode(filp));
	int read_len = count;

	if (!ts_data || !ts_data->ta_buf)
		return -EINVAL;

	if (read_len > FTS_MAX_TOUCH_BUF)
		read_len = FTS_MAX_TOUCH_BUF;

	ret = wait_event_interruptible(ts_data->ts_waitqueue, ts_data->ta_flag);
	if (ret)
		return ret;

	ts_data->ta_flag = 0;
	if (copy_to_user(buff, ts_data->ta_buf, read_len)) {
		FTS_ERROR("copy to user fail");
		return -EFAULT;
	}

	return read_len;
}

static const struct proc_ops fts_procta_fops = {
	.proc_open = fts_ta_open,
	.proc_release = fts_ta_release,
	.proc_read = fts_ta_read,
};

static ssize_t fts_debug_write(struct file *filp, const char __user *buff,
			       size_t count, loff_t *ppos)
{
	u8 *writebuf = NULL;
	u8 tmpbuf[64] = { 0 };
	int buflen = count;
	int writelen = 0;
	int ret = 0;
	char tmp[25] = { 0 };
	struct fts_ts_data *ts_data = pde_data(file_inode(filp));

	if ((buflen <= 0) || (buflen > 4096)) {
		FTS_ERROR("apk write len(%d) fail", buflen);
		return -EINVAL;
	}

	if (buflen > sizeof(tmpbuf)) {
		writebuf = kzalloc(buflen, GFP_KERNEL);
		if (!writebuf)
			return -ENOMEM;
	} else {
		writebuf = tmpbuf;
	}

	if (copy_from_user(writebuf, buff, buflen)) {
		FTS_ERROR("copy from user error");
		ret = -EFAULT;
		goto out;
	}

	fts_esdcheck_proc_busy(ts_data, true);
	proc_opmode = writebuf[0];

	switch (proc_opmode) {
	case PROC_SET_TEST_FLAG:
		FTS_DEBUG("[APK]: PROC_SET_TEST_FLAG = %x", writebuf[1]);
		if (writebuf[1] == 0)
			fts_esdcheck_switch(ts_data, true);
		else
			fts_esdcheck_switch(ts_data, false);
		break;

	case PROC_READ_REGISTER:
		proc_cmd[0] = writebuf[1];
		break;

	case PROC_WRITE_REGISTER:
		ret = fts_write_reg(writebuf[1], writebuf[2]);
		if (ret < 0) {
			FTS_ERROR("PROC_WRITE_REGISTER write error");
			goto err_esd;
		}
		break;

	case PROC_READ_DATA:
		writelen = buflen - 1;
		if (writelen >= 1) {
			proc_cmd[0] = writebuf[1];
			ret = fts_write(writebuf + 1, writelen);
			if (ret < 0)
				FTS_ERROR("PROC_READ_DATA write error");
		}
		break;

	case PROC_WRITE_DATA:
		writelen = buflen - 1;
		ret = fts_write(writebuf + 1, writelen);
		if (ret < 0) {
			FTS_ERROR("PROC_WRITE_DATA write error");
			goto err_esd;
		}
		break;

	case PROC_HW_RESET:
		if (buflen < sizeof(tmp)) {
			memcpy(tmp, writebuf + 1, buflen - 1);
			tmp[buflen - 1] = '\0';
			if (strncmp(tmp, "focal_driver", 12) == 0) {
				FTS_INFO("APK execute HW Reset");
				fts_reset_proc(0);
			}
		}
		break;

	case PROC_ENTER_TEST_ENVIRONMENT:
		FTS_DEBUG("[APK]: PROC_ENTER_TEST_ENVIRONMENT = %x", writebuf[1]);
		if (writebuf[1] == 0)
			fts_enter_test_environment(false);
		else
			fts_enter_test_environment(true);
		break;

	case PROC_WRITE_DATA_DIRECT:
		writelen = buflen - 1;
		ret = fts_bus_transfer_direct(writebuf + 1, writelen, NULL, 0);
		if (ret < 0) {
			FTS_ERROR("PROC_WRITE_DATA_DIRECT write error");
			goto err_esd;
		}
		break;

	case PROC_CONFIGURE:
		fts_bus_configure(ts_data, writebuf + 1, buflen - 1);
		break;

	default:
		break;
	}

	ret = buflen;
err_esd:
	fts_esdcheck_proc_busy(ts_data, false);
out:
	if (buflen > sizeof(tmpbuf))
		kfree(writebuf);
	return ret;
}

static ssize_t fts_debug_read(struct file *filp, char __user *buff,
			      size_t count, loff_t *ppos)
{
	int ret = 0;
	int num_read_chars = 0;
	int buflen = count;
	u8 *readbuf = NULL;
	u8 tmpbuf[64] = { 0 };
	struct fts_ts_data *ts_data = pde_data(file_inode(filp));

	if ((buflen <= 0) || (buflen > 4096)) {
		FTS_ERROR("apk read len(%d) fail", buflen);
		return -EINVAL;
	}

	if (buflen > sizeof(tmpbuf)) {
		readbuf = kzalloc(buflen, GFP_KERNEL);
		if (!readbuf)
			return -ENOMEM;
	} else {
		readbuf = tmpbuf;
	}

	fts_esdcheck_proc_busy(ts_data, true);

	switch (proc_opmode) {
	case PROC_READ_REGISTER:
		num_read_chars = 1;
		ret = fts_read_reg(proc_cmd[0], &readbuf[0]);
		if (ret < 0) {
			FTS_ERROR("PROC_READ_REGISTER read error");
			goto err_esd;
		}
		break;

	case PROC_READ_DATA:
		num_read_chars = buflen;
		ret = fts_read(proc_cmd, 1, readbuf, num_read_chars);
		if (ret < 0) {
			FTS_ERROR("PROC_READ_DATA read error");
			goto err_esd;
		}
		break;

	case PROC_READ_DATA_DIRECT:
		num_read_chars = buflen;
		ret = fts_bus_transfer_direct(proc_cmd, 1, readbuf, num_read_chars);
		if (ret < 0) {
			FTS_ERROR("PROC_READ_DATA_DIRECT read error");
			goto err_esd;
		}
		break;

	default:
		break;
	}

	ret = num_read_chars;
err_esd:
	fts_esdcheck_proc_busy(ts_data, false);

	if (ret > 0) {
		if (copy_to_user(buff, readbuf, ret)) {
			FTS_ERROR("copy to user error");
			ret = -EFAULT;
		}
	}

	if (buflen > sizeof(tmpbuf))
		kfree(readbuf);
	return ret;
}

static const struct proc_ops fts_proc_fops = {
	.proc_read = fts_debug_read,
	.proc_write = fts_debug_write,
};

int fts_create_apk_debug_channel(struct fts_ts_data *ts_data)
{
	ts_data->proc = proc_create_data(PROC_NAME, 0777, NULL, &fts_proc_fops, ts_data);
	if (!ts_data->proc) {
		FTS_ERROR("create proc entry fail");
		return -ENOMEM;
	}

	ts_data->proc_ta = proc_create_data(PROC_TA_NAME, 0777, NULL, &fts_procta_fops, ts_data);
	if (!ts_data->proc_ta)
		FTS_ERROR("create proc_ta entry fail");

	FTS_INFO("Create proc entry success!");
	return 0;
}

void fts_release_apk_debug_channel(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	if (ts_data->proc) {
		proc_remove(ts_data->proc);
		ts_data->proc = NULL;
	}
	if (ts_data->proc_ta) {
		proc_remove(ts_data->proc_ta);
		ts_data->proc_ta = NULL;
	}
	FTS_FUNC_EXIT();
}

int fts_set_edge_reject_switch(u8 mode)
{
	int ret = 0;

	FTS_INFO("edge reject mode=%d", mode);
	if (mode > 3) {
		FTS_ERROR("invalid edge reject mode=%d", mode);
		return -EINVAL;
	}

	ret = fts_write_reg(FTS_REG_EDGE_REJECT, mode);
	if (ret < 0) {
		FTS_ERROR("write edge reject reg(0x8C) fail, ret=%d", ret);
		return ret;
	}

	return 0;
}

/* Sysfs attributes */
static ssize_t fts_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	ssize_t num_read_chars = 0;
	u8 fwver = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	if (fts_read_reg(FTS_REG_FW_VER, &fwver) < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "FW Ver:0x%02x\n", fwver);
	mutex_unlock(&ts_data->input_dev->mutex);
	return num_read_chars;
}

static ssize_t fts_tpfwver_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_tprwreg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	int i = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (rw_op.opbuf_len < 0) {
		count = snprintf(buf, PAGE_SIZE, "Invalid cmd line\n");
	} else if (rw_op.opbuf_len == 1) {
		if (rw_op.type == 0)
			count = snprintf(buf, PAGE_SIZE, "Read %02X: %02X\n", rw_op.reg, rw_op.val);
		else
			count = snprintf(buf, PAGE_SIZE, "Write %02X, %02X %s\n",
					 rw_op.reg, rw_op.val, rw_op.res ? "failed" : "success");
	} else {
		if (rw_op.type == 0 && rw_op.opbuf) {
			count = snprintf(buf, PAGE_SIZE, "Read %02X:", rw_op.reg);
			for (i = 0; i < rw_op.opbuf_len && count < PAGE_SIZE - 4; i++)
				count += snprintf(buf + count, PAGE_SIZE - count, " %02X", rw_op.opbuf[i]);
			count += snprintf(buf + count, PAGE_SIZE - count, "\n");
		} else {
			count = snprintf(buf, PAGE_SIZE, "Write %02X %s\n",
					 rw_op.reg, rw_op.res ? "failed" : "success");
		}
	}
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_tprwreg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	unsigned int reg = 0, val = 0;
	u8 rval = 0;
	int ret = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	if (sscanf(buf, "%x %x", &reg, &val) == 2) {
		rw_op.type = 1;
		rw_op.reg = reg & 0xFF;
		rw_op.val = val & 0xFF;
		rw_op.opbuf_len = 1;
		rw_op.res = fts_write_reg(rw_op.reg, rw_op.val);
	} else if (sscanf(buf, "%x", &reg) == 1) {
		rw_op.type = 0;
		rw_op.reg = reg & 0xFF;
		rw_op.opbuf_len = 1;
		ret = fts_read_reg(rw_op.reg, &rval);
		rw_op.val = rval;
		rw_op.res = ret;
		if (ret >= 0)
			FTS_INFO("read 0x%02x, %d bytes successful", rw_op.reg, 1);
	} else {
		rw_op.opbuf_len = -1;
	}
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_dumpreg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	fts_read_reg(FTS_REG_POWER_MODE, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Power Mode:0x%02x\n", val);
	fts_read_reg(FTS_REG_FW_VER, &val);
	count += snprintf(buf + count, PAGE_SIZE, "FW Ver:0x%02x\n", val);
	fts_read_reg(FTS_REG_LICESS_VER, &val);
	count += snprintf(buf + count, PAGE_SIZE, "LIC Ver:0x%02x\n", val);
	fts_read_reg(FTS_REG_VENDOR_ID, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Vendor ID:0x%02x\n", val);
	fts_read_reg(FTS_REG_LCD_BUSY_NUM, &val);
	count += snprintf(buf + count, PAGE_SIZE, "LCD Busy Num:0x%02x\n", val);
	fts_read_reg(FTS_REG_GESTURE_EN, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Gesture Mode:0x%02x\n", val);
	fts_read_reg(FTS_REG_CHARGER_MODE_EN, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Charge Mode:0x%02x\n", val);
	fts_read_reg(FTS_REG_FLOW_WORK_CNT, &val);
	count += snprintf(buf + count, PAGE_SIZE, "ESD:0x%02x\n", val);
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_dumpreg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_fwupgradebin_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

static ssize_t fts_fwupgradebin_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char fwname[128] = { 0 };
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if ((count <= 1) || (count >= sizeof(fwname)))
		return -EINVAL;

	memcpy(fwname, buf, count);
	fwname[count - 1] = '\0';

	mutex_lock(&ts_data->input_dev->mutex);
	fts_upgrade_bin(fwname, false);
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_fwforceupg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

static ssize_t fts_fwforceupg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char fwname[128] = { 0 };
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if ((count <= 1) || (count >= sizeof(fwname)))
		return -EINVAL;

	memcpy(fwname, buf, count);
	fwname[count - 1] = '\0';

	mutex_lock(&ts_data->input_dev->mutex);
	fts_upgrade_bin(fwname, true);
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_driverinfo_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	mutex_lock(&ts_data->input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "Driver Ver:%s\n", FTS_DRIVER_VERSION);
	count += snprintf(buf + count, PAGE_SIZE, "Resolution:(%d,%d)-(%d,%d)\n",
			  pdata->x_min, pdata->y_min, pdata->x_max, pdata->y_max);
	count += snprintf(buf + count, PAGE_SIZE, "Max Touchs:%d\n", pdata->max_touch_number);
	count += snprintf(buf + count, PAGE_SIZE, "reset gpio:%d,int gpio:%d,irq:%d\n",
			  pdata->reset_gpio, pdata->irq_gpio, ts_data->irq);
	count += snprintf(buf + count, PAGE_SIZE, "IC ID:0x%02x%02x\n",
			  ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_driverinfo_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_hw_reset_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	ssize_t count = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	fts_reset_proc(0);
	count = snprintf(buf, PAGE_SIZE, "hw reset executed\n");
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_hw_reset_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_irq_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct irq_desc *desc = irq_to_desc(ts_data->irq);

	return snprintf(buf, PAGE_SIZE, "irq_depth:%d\n", desc ? desc->depth : -1);
}

static ssize_t fts_irq_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
		fts_irq_enable();
	else if (FTS_SYSFS_ECHO_OFF(buf))
		fts_irq_disable();
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_bootmode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	ssize_t count = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "fw_is_running:%d\n", ts_data->fw_is_running);
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_bootmode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
		ts_data->fw_is_running = 1;
	else if (FTS_SYSFS_ECHO_OFF(buf))
		ts_data->fw_is_running = 0;
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_tpbuf_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int count = 0;
	int i = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "touch point buffer:\n");
	for (i = 0; i < ts_data->touch_size && count < PAGE_SIZE - 4; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%02x ", ts_data->point_buf[i]);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_tpbuf_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return -EPERM;
}

static ssize_t fts_log_level_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "log level:%d\n", ts_data->log_level);
}

static ssize_t fts_log_level_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int value = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	if (sscanf(buf, "%d", &value) == 1)
		ts_data->log_level = value;
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_pen_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "pen event:%s\n",
			ts_data->pen_support ? "On" : "Off");
}

static ssize_t fts_pen_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
		ts_data->pen_support = true;
	else if (FTS_SYSFS_ECHO_OFF(buf))
		ts_data->pen_support = false;
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_touchsize_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "touch size:%d\n", ts_data->touch_size);
}

static ssize_t fts_touchsize_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int value = 0;

	mutex_lock(&ts_data->input_dev->mutex);
	if (sscanf(buf, "%d", &value) == 1) {
		if ((value > 0) && (value <= FTS_MAX_TOUCH_BUF))
			ts_data->touch_size = value;
		else
			FTS_DEBUG("touch size:%d invalid", value);
	}
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_tamode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "ta mode:%s\n",
			ts_data->ta_mode ? "On" : "Off");
}

static ssize_t fts_tamode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
		ts_data->ta_mode = true;
	else if (FTS_SYSFS_ECHO_OFF(buf))
		ts_data->ta_mode = false;
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static DEVICE_ATTR(fts_fw_version, 0644, fts_tpfwver_show, fts_tpfwver_store);
static DEVICE_ATTR(fts_rw_reg, 0644, fts_tprwreg_show, fts_tprwreg_store);
static DEVICE_ATTR(fts_dump_reg, 0644, fts_dumpreg_show, fts_dumpreg_store);
static DEVICE_ATTR(fts_upgrade_bin, 0644, fts_fwupgradebin_show, fts_fwupgradebin_store);
static DEVICE_ATTR(fts_force_upgrade, 0644, fts_fwforceupg_show, fts_fwforceupg_store);
static DEVICE_ATTR(fts_driver_info, 0644, fts_driverinfo_show, fts_driverinfo_store);
static DEVICE_ATTR(fts_hw_reset, 0644, fts_hw_reset_show, fts_hw_reset_store);
static DEVICE_ATTR(fts_irq, 0644, fts_irq_show, fts_irq_store);
static DEVICE_ATTR(fts_boot_mode, 0644, fts_bootmode_show, fts_bootmode_store);
static DEVICE_ATTR(fts_touch_point, 0644, fts_tpbuf_show, fts_tpbuf_store);
static DEVICE_ATTR(fts_log_level, 0644, fts_log_level_show, fts_log_level_store);
static DEVICE_ATTR(fts_pen, 0644, fts_pen_show, fts_pen_store);
static DEVICE_ATTR(fts_touch_size, 0644, fts_touchsize_show, fts_touchsize_store);
static DEVICE_ATTR(fts_ta_mode, 0644, fts_tamode_show, fts_tamode_store);

static struct attribute *fts_attributes[] = {
	&dev_attr_fts_fw_version.attr,
	&dev_attr_fts_rw_reg.attr,
	&dev_attr_fts_dump_reg.attr,
	&dev_attr_fts_upgrade_bin.attr,
	&dev_attr_fts_force_upgrade.attr,
	&dev_attr_fts_driver_info.attr,
	&dev_attr_fts_hw_reset.attr,
	&dev_attr_fts_irq.attr,
	&dev_attr_fts_boot_mode.attr,
	&dev_attr_fts_touch_point.attr,
	&dev_attr_fts_log_level.attr,
	&dev_attr_fts_pen.attr,
	&dev_attr_fts_touch_size.attr,
	&dev_attr_fts_ta_mode.attr,
	NULL,
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes,
};

int fts_create_sysfs(struct fts_ts_data *ts_data)
{
	int ret = 0;

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_attribute_group);
	if (ret) {
		FTS_ERROR("[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_attribute_group);
		return -ENOMEM;
	}

	FTS_INFO("[EX]: sysfs_create_group() succeeded!!");
	return 0;
}

int fts_remove_sysfs(struct fts_ts_data *ts_data)
{
	sysfs_remove_group(&ts_data->dev->kobj, &fts_attribute_group);
	return 0;
}
