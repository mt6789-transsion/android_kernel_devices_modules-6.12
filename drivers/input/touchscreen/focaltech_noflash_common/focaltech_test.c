// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech Factory Self-Test Core & Procfs/Sysfs Interface
 */
#include "focaltech_test.h"

struct fts_test *fts_ftest;
static u8 fts_test_result;

void sys_delay(int ms)
{
	msleep(ms);
}

int fts_abs(int value)
{
	return (value < 0) ? -value : value;
}

void *fts_malloc(size_t size)
{
	return vmalloc(size);
}

void fts_free_proc(void *p)
{
	vfree(p);
}

int fts_test_read_reg(u8 addr, u8 *val)
{
	return fts_read_reg(addr, val);
}

int fts_test_write_reg(u8 addr, u8 val)
{
	return fts_write_reg(addr, val);
}

int fts_test_read(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;
	const int max_len = 256;

	packet_num = byte_num / max_len;
	packet_remainder = byte_num % max_len;
	if (packet_remainder)
		packet_num++;

	packet_length = (byte_num < max_len) ? byte_num : max_len;
	ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
	if (ret < 0) {
		FTS_TEST_ERROR("read buffer fail");
		return ret;
	}

	for (i = 1; i < packet_num; i++) {
		offset += max_len;
		if ((i == (packet_num - 1)) && packet_remainder)
			packet_length = packet_remainder;
		ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
		if (ret < 0) {
			FTS_TEST_ERROR("read buffer fail");
			return ret;
		}
	}

	return 0;
}

int fts_test_write(u8 addr, u8 *writebuf, int writelen)
{
	u8 *data = NULL;
	int ret = 0;

	data = kzalloc(writelen + 1, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data[0] = addr;
	if (writebuf && writelen > 0)
		memcpy(&data[1], writebuf, writelen);

	ret = fts_write(data, writelen + 1);
	kfree(data);
	return ret;
}

int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;

	FTS_TEST_FUNC_ENTER();
	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE_WORK_VALUE))
		return 0;

	for (i = 0; i < 5; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, FTS_REG_WORKMODE_WORK_VALUE);
		if (ret >= 0) {
			sys_delay(20);
			ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
			if ((ret >= 0) && (mode == FTS_REG_WORKMODE_WORK_VALUE)) {
				FTS_TEST_INFO("enter work mode success");
				return 0;
			}
		}
		sys_delay(50);
	}

	FTS_TEST_ERROR("enter work mode fail");
	return -EIO;
}

int enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;

	FTS_TEST_FUNC_ENTER();
	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE_FACTORY_VALUE))
		return 0;

	for (i = 0; i < 5; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, FTS_REG_WORKMODE_FACTORY_VALUE);
		if (ret >= 0) {
			sys_delay(20);
			ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);
			if ((ret >= 0) && (mode == FTS_REG_WORKMODE_FACTORY_VALUE)) {
				FTS_TEST_INFO("enter factory mode success");
				sys_delay(200);
				return 0;
			}
		}
		sys_delay(50);
	}

	FTS_TEST_ERROR("failed to enter factory mode,ret=%d", ret);
	return -EIO;
}

int read_mass_data(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	data = kzalloc(byte_num, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = fts_test_read(addr, data, byte_num);
	if (ret < 0) {
		FTS_TEST_ERROR("read mass data fail");
		goto out;
	}

	for (i = 0; i < byte_num; i += 2)
		buf[i >> 1] = (int)(s16)((data[i] << 8) + data[i + 1]);

out:
	kfree(data);
	return ret;
}

int read_mass_data_u16(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	data = kzalloc(byte_num, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = fts_test_read(addr, data, byte_num);
	if (ret < 0) {
		FTS_TEST_ERROR("read mass data fail");
		goto out;
	}

	for (i = 0; i < byte_num; i += 2)
		buf[i >> 1] = (int)(u16)((data[i] << 8) + data[i + 1]);

out:
	kfree(data);
	return ret;
}

int wait_state_update(u8 retval)
{
	int ret = 0;
	int times = 0;
	u8 state = 0xFF;

	while (times++ < 20) {
		sys_delay(10);
		ret = fts_test_read_reg(0x06, &state);
		if ((ret >= 0) && (state == retval))
			return 0;
	}

	FTS_TEST_ERROR("wait state update timeout, state=0x%02x", state);
	return -EIO;
}

int start_scan(void)
{
	int ret = 0;
	u8 addr = DEVIDE_MODE_ADDR;
	u8 val = 0;
	int times = 0;

	ret = fts_test_read_reg(addr, &val);
	if (ret < 0)
		return ret;

	val |= 0x80;
	ret = fts_test_write_reg(addr, val);
	if (ret < 0)
		return ret;

	while (times++ < 50) {
		sys_delay(10);
		ret = fts_test_read_reg(addr, &val);
		if ((ret >= 0) && ((val & 0x80) == 0))
			return 0;
	}

	FTS_TEST_ERROR("start scan timeout");
	return -EIO;
}

int get_rawdata(int *data)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	int byte_num = tdata->node.node_num * 2;

	ret = fts_test_write_reg(REG_LINE_NUM, 0xAD);
	if (ret < 0)
		return ret;

	ret = read_mass_data(REG_RAWBUF0, byte_num, data);
	if (ret < 0) {
		FTS_TEST_ERROR("read rawdata fail");
		return ret;
	}

	return 0;
}

int chip_clb(void)
{
	int ret = 0;
	u8 val = 0;
	int times = 0;

	ret = fts_test_write_reg(REG_CLB, 0x04);
	if (ret < 0) {
		FTS_TEST_ERROR("write start clb fail");
		return ret;
	}

	while (times++ < 50) {
		sys_delay(20);
		ret = fts_test_read_reg(REG_CLB, &val);
		if ((ret >= 0) && (val == 0x02))
			return 0;
	}

	FTS_TEST_ERROR("chip clb timeout");
	return -EIO;
}

int get_cb_incell(u16 saddr, int byte_num, int *cb_buf)
{
	int ret = 0;
	int i = 0;
	u8 *cb = NULL;

	cb = kzalloc(byte_num, GFP_KERNEL);
	if (!cb)
		return -ENOMEM;

	ret = fts_test_write_reg(REG_CB_ADDR_H, (saddr >> 8) & 0xFF);
	ret |= fts_test_write_reg(REG_CB_ADDR_L, saddr & 0xFF);
	if (ret < 0)
		goto out;

	ret = fts_test_read(REG_CB_BUF0, cb, byte_num);
	if (ret < 0)
		goto out;

	for (i = 0; i < byte_num; i++)
		cb_buf[i] = cb[i];

out:
	kfree(cb);
	return ret;
}

int short_get_adcdata_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf)
{
	int ret = 0;
	int times = 0;
	u8 short_state = 0;

	ret = fts_test_write_reg(FACTORY_REG_SHORT_TEST_EN, 0x01);
	if (ret < 0) {
		FTS_TEST_ERROR("write short test mode fail");
		return ret;
	}

	for (times = 0; times < 50; times++) {
		sys_delay(50);
		ret = fts_test_read_reg(FACTORY_REG_SHORT_TEST_STATE, &short_state);
		if ((ret >= 0) && (short_state == retval))
			break;
	}
	if (times >= 50) {
		FTS_TEST_ERROR("short test timeout, state=0x%02x", short_state);
		return -EIO;
	}

	ret = read_mass_data_u16(FACTORY_REG_SHORT_ADDR, byte_num, adc_buf);
	if (ret < 0) {
		FTS_TEST_ERROR("get short adc data fail");
		return ret;
	}

	return 0;
}

bool compare_data(int *data, int min, int max, int min_vk, int max_vk, bool key)
{
	int i = 0;
	bool result = true;
	struct fts_test *tdata = fts_ftest;
	int va_len = tdata->node.node_num - tdata->node.key_num;

	for (i = 0; i < va_len; i++) {
		if ((data[i] < min) || (data[i] > max)) {
			FTS_TEST_DBG("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)",
				     i / tdata->node.rx_num + 1,
				     i % tdata->node.rx_num + 1,
				     data[i], min, max);
			result = false;
		}
	}

	if (key) {
		for (i = va_len; i < tdata->node.node_num; i++) {
			if ((data[i] < min_vk) || (data[i] > max_vk))
				result = false;
		}
	}

	return result;
}

void fts_test_save_data(char *name, int code, int *data, int datacnt,
			bool mc_sc, bool key, bool result)
{
	struct fts_test *tdata = fts_ftest;
	int i = 0;
	int len = 0;

	if (!tdata || !tdata->csv_buffer)
		return;

	len = tdata->csv_length;
	len += snprintf(tdata->csv_buffer + len, CSV_BUFFER_LEN - len,
			"%s, %s\n", name, result ? "PASS" : "NG");
	for (i = 0; i < datacnt && len < CSV_BUFFER_LEN - 16; i++) {
		len += snprintf(tdata->csv_buffer + len, CSV_BUFFER_LEN - len,
				"%d%s", data[i],
				((i + 1) % tdata->node.rx_num == 0) ? "\n" : ", ");
	}
	len += snprintf(tdata->csv_buffer + len, CSV_BUFFER_LEN - len, "\n");
	tdata->csv_length = len;
}

static int get_tx_rx_num(void)
{
	int ret = 0;
	u8 tx = 0, rx = 0;
	struct fts_test *tdata = fts_ftest;

	ret = enter_factory_mode();
	if (ret < 0)
		return ret;

	ret = fts_test_read_reg(REG_TX_NUM, &tx);
	ret |= fts_test_read_reg(REG_RX_NUM, &rx);
	if (ret < 0 || tx == 0 || rx == 0) {
		FTS_TEST_ERROR("get channel num fail");
		return -EIO;
	}

	tdata->node.tx_num = tx;
	tdata->node.rx_num = rx;
	tdata->node.channel_num = tx + rx;
	tdata->node.node_num = tx * rx;
	tdata->node.key_num = 0;
	FTS_TEST_INFO("node_num:%d, tx:%d, rx:%d, key:%d",
		      tdata->node.node_num, tx, rx, tdata->node.key_num);
	return 0;
}

static int fts_test_entry(char *ini_file_name)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;

	FTS_TEST_INFO("ini_file_name:%s", ini_file_name ? ini_file_name : "default");
	if (!tdata || !tdata->func) {
		FTS_TEST_ERROR("tdata/func is null");
		return -EINVAL;
	}

	tdata->csv_length = 0;
	tdata->result = RESULT_NG;

	ret = get_tx_rx_num();
	if (ret < 0)
		goto out;

	if (tdata->func->start_test)
		ret = tdata->func->start_test();

	tdata->result = (ret == 0) ? RESULT_PASS : RESULT_NG;
	fts_test_result = (ret == 0) ? 1 : 0;

out:
	enter_work_mode();
	return ret;
}

int tran_fts_get_SelftestResult(void)
{
	int ret = 0;
	char fwname[128] = { 0 };
	struct fts_ts_data *ts_data = fts_data;
	int supplier_id = 1;

	if (!ts_data)
		return 0;

	if (fts_noflash_controller.ts && fts_noflash_controller.ts->supplier)
		supplier_id = fts_noflash_controller.ts->supplier->id;

	snprintf(fwname, sizeof(fwname), "%s_0%d.ini", "Conf_MultipleTest", supplier_id);
	FTS_TEST_DBG("fwname:%s.", fwname);

	mutex_lock(&ts_data->input_dev->mutex);
	fts_irq_disable();
	fts_esdcheck_switch(ts_data, false);

	ret = fts_enter_test_environment(true);
	if (ret < 0) {
		FTS_ERROR("enter test environment fail");
	} else {
		ret = fts_test_entry(fwname);
		fts_enter_test_environment(false);
	}

	fts_esdcheck_switch(ts_data, true);
	fts_irq_enable();
	mutex_unlock(&ts_data->input_dev->mutex);

	return (ret == 0) ? 1 : 0;
}

static int fts_csv_show(struct seq_file *s, void *v)
{
	struct fts_test *tdata = s->private;

	if (!tdata || !tdata->csv_buffer || (strlen(tdata->csv_buffer) == 0)) {
		seq_printf(s, "no csv data, please do test first\n");
		return 0;
	}

	seq_printf(s, "%s", tdata->csv_buffer);
	return 0;
}

static int fts_csv_open(struct inode *inode, struct file *file)
{
	return single_open(file, fts_csv_show, pde_data(inode));
}

static const struct proc_ops fts_proccsv_fops = {
	.proc_open = fts_csv_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t fts_test_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "test result:%d\n", fts_test_result);
}

static ssize_t fts_test_store(struct device *dev,
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
	fts_irq_disable();
	fts_esdcheck_switch(ts_data, false);
	if (fts_enter_test_environment(true) == 0) {
		fts_test_entry(fwname);
		fts_enter_test_environment(false);
	}
	fts_esdcheck_switch(ts_data, true);
	fts_irq_enable();
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static DEVICE_ATTR(fts_test, 0644, fts_test_show, fts_test_store);

static struct attribute *fts_test_attributes[] = {
	&dev_attr_fts_test.attr,
	NULL,
};

static struct attribute_group fts_test_attribute_group = {
	.attrs = fts_test_attributes,
};

int fts_test_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_TEST_FUNC_ENTER();
	fts_ftest = kzalloc(sizeof(*fts_ftest), GFP_KERNEL);
	if (!fts_ftest) {
		FTS_TEST_ERROR("malloc memory for test fail");
		return -ENOMEM;
	}

	fts_ftest->ts_data = ts_data;
	fts_ftest->func = &test_func_ft8725;
	fts_ftest->csv_buffer = vmalloc(CSV_BUFFER_LEN);
	if (fts_ftest->csv_buffer)
		memset(fts_ftest->csv_buffer, 0, CSV_BUFFER_LEN);

	/* Default FT8725 thresholds */
	fts_ftest->item.short_test = 1;
	fts_ftest->item.open_test = 1;
	fts_ftest->item.cb_test = 1;
	fts_ftest->item.rawdata_test = 1;
	fts_ftest->item.lcdnoise_test = 1;
	fts_ftest->thr.short_res_min = 200;
	fts_ftest->thr.open_cb_min = 10;
	fts_ftest->thr.cb_min = 3;
	fts_ftest->thr.cb_max = 100;
	fts_ftest->thr.rawdata_min = 3000;
	fts_ftest->thr.rawdata_max = 15000;
	fts_ftest->thr.lcdnoise_frame = 200;
	fts_ftest->thr.lcdnoise_coefficient = 50;

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	if (ret) {
		FTS_TEST_ERROR("sysfs(test) create fail");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	} else {
		FTS_TEST_INFO("sysfs(test) create successfully");
	}

	fts_ftest->proc_csv = proc_create_data("fts_test_csv", 0644, NULL,
					       &fts_proccsv_fops, fts_ftest);
	if (!fts_ftest->proc_csv)
		FTS_TEST_ERROR("create fts_test_csv proc fail");

	FTS_TEST_FUNC_EXIT();
	return 0;
}

int fts_test_exit(struct fts_ts_data *ts_data)
{
	FTS_TEST_FUNC_ENTER();
	sysfs_remove_group(&ts_data->dev->kobj, &fts_test_attribute_group);
	if (fts_ftest) {
		if (fts_ftest->proc_csv) {
			proc_remove(fts_ftest->proc_csv);
			fts_ftest->proc_csv = NULL;
		}
		if (fts_ftest->csv_buffer) {
			vfree(fts_ftest->csv_buffer);
			fts_ftest->csv_buffer = NULL;
		}
		kfree(fts_ftest);
		fts_ftest = NULL;
	}
	FTS_TEST_FUNC_EXIT();
	return 0;
}
