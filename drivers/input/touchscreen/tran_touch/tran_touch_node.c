// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion touch panel procfs nodes & external command handler (X6837 / MT6789)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "tran_touch_core.h"

#define SELFTEST_DATA_BUF_SIZE	40960

static ssize_t tp_game_mode_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	static bool finished;
	char *kbuf;
	ssize_t len;

	if (finished) {
		tran_info("read END\n");
		finished = false;
		return 0;
	}

	finished = true;
	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	len = sprintf(kbuf, "%d\n", g_tran_ts.game_mode);
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	if (copy_to_user(buf, kbuf, len)) {
		kfree(kbuf);
		return len;
	}

	*ppos += len;
	kfree(kbuf);
	tran_info("--\n");
	return len;
}

static ssize_t tp_game_mode_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char kbuf[2] = "0";
	int val = 0;
	int ret;

	if (!ctrl) {
		tran_err("controller is null\n");
		return -EINVAL;
	}

	if (count < 1 || count > 2) {
		tran_err("Invalid value! count = %zu\n", count);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, buf, 1)) {
		tran_err("copy_from_user fail.\n");
		return -1;
	}

	ret = sscanf(kbuf, "%d", &val);
	if (ret != 1) {
		tran_err("Invalid value! ret = %d\n", ret);
		return -EINVAL;
	}

	if ((unsigned int)val >= 6) {
		tran_err("Invalid value! ret = %d, tran_report_rate val = %d\n",
			 ret, val);
		return -EINVAL;
	}

	tran_info("tran_report_rate = %d\n", val);
	g_tran_ts.game_mode = (u8)val;

	if (g_tran_ts.state == TRAN_SUSPEND_MODE) {
		tran_err("TP already suspended do not set game_mode\n");
		return count;
	}

	if (ctrl->ops && ctrl->ops->set_ts_mode)
		ctrl->ops->set_ts_mode(TRAN_MODE_GAME_MODE, (unsigned int)val);

	return count;
}

static const struct proc_ops tp_game_mode_fops = {
	.proc_read = tp_game_mode_read,
	.proc_write = tp_game_mode_write,
};

static ssize_t tp_report_rate_read(struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	static bool finished;
	char *kbuf;
	ssize_t len;

	if (finished) {
		tran_info("read END\n");
		finished = false;
		return 0;
	}

	finished = true;
	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	len = sprintf(kbuf, "%d\n", g_tran_ts.report_rate);
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	if (copy_to_user(buf, kbuf, len)) {
		kfree(kbuf);
		return len;
	}

	*ppos += len;
	kfree(kbuf);
	tran_info("--\n");
	return len;
}

static ssize_t tp_report_rate_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char kbuf[2] = "0";
	int val = 0;
	int ret;

	if (!ctrl) {
		tran_err("controller is null\n");
		return -EINVAL;
	}

	if (count < 1 || count > 2) {
		tran_err("Invalid value! count = %zu\n", count);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, buf, 1)) {
		tran_err("copy_from_user fail.\n");
		return -1;
	}

	ret = sscanf(kbuf, "%d", &val);
	if (ret != 1) {
		tran_err("Invalid value! ret = %d\n", ret);
		return -EINVAL;
	}

	if ((unsigned int)val >= 6) {
		tran_err("Invalid value! ret = %d, tran_report_rate val = %d\n",
			 ret, val);
		return -EINVAL;
	}

	tran_info("tran_report_rate = %d\n", val);
	g_tran_ts.report_rate = (u8)val;

	if (g_tran_ts.state == TRAN_SUSPEND_MODE) {
		tran_err("TP already suspended do not set report_rate\n");
		return count;
	}

	if (ctrl->ops && ctrl->ops->set_ts_mode)
		ctrl->ops->set_ts_mode(TRAN_MODE_REPORT_RATE, (unsigned int)val);

	return count;
}

static const struct proc_ops tp_report_rate_fops = {
	.proc_read = tp_report_rate_read,
	.proc_write = tp_report_rate_write,
};

static ssize_t tp_transient_report_rate_read(struct file *file,
					     char __user *buf,
					     size_t count, loff_t *ppos)
{
	static bool finished;
	char *kbuf;
	ssize_t len;

	if (finished) {
		tran_info("read END\n");
		finished = false;
		return 0;
	}

	finished = true;
	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	len = sprintf(kbuf, "%d\n", g_tran_ts.transient_report_rate);
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	if (copy_to_user(buf, kbuf, len)) {
		kfree(kbuf);
		return len;
	}

	*ppos += len;
	kfree(kbuf);
	tran_info("--\n");
	return len;
}

static ssize_t tp_transient_report_rate_write(struct file *file,
					      const char __user *buf,
					      size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char kbuf[2] = "0";
	int val = 0;
	int ret;

	if (!ctrl) {
		tran_err("controller is null\n");
		return -EINVAL;
	}

	if (count < 1 || count > 2) {
		tran_err("Invalid value! count = %zu\n", count);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, buf, 1)) {
		tran_err("copy_from_user fail.\n");
		return -1;
	}

	ret = sscanf(kbuf, "%d", &val);
	if (ret != 1) {
		tran_err("Invalid value! ret = %d\n", ret);
		return -EINVAL;
	}

	if ((unsigned int)val >= 2) {
		tran_err("Invalid value! ret = %d, tran_transient_report_rate val = %d\n",
			 ret, val);
		return -EINVAL;
	}

	tran_info("tran_transient_report_rate = %d\n", val);
	g_tran_ts.transient_report_rate = (u8)val;

	if (g_tran_ts.state == TRAN_SUSPEND_MODE) {
		tran_err("TP already suspended do not tran_transient_report_rate\n");
		return count;
	}

	if (ctrl->ops && ctrl->ops->set_ts_mode)
		ctrl->ops->set_ts_mode(TRAN_MODE_TRANSIENT_REPORT_RATE,
				       (unsigned int)val);

	return count;
}

static const struct proc_ops tp_transient_report_rate_fops = {
	.proc_read = tp_transient_report_rate_read,
	.proc_write = tp_transient_report_rate_write,
};

static ssize_t tp_game_state_read(struct file *file, char __user *buf,
				  size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t len;
	u8 state = g_tran_ts.game_state;

	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	len = sprintf(kbuf, "%d\n", state);
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	if (!copy_to_user(buf, kbuf, len))
		*ppos += len;

	kfree(kbuf);
	return len;
}

static ssize_t tp_game_state_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl;
	char kbuf[2] = "0";
	int tmp = 0;
	u8 val;
	int ret;

	if (count < 1 || count > 2) {
		tran_err("Invalid value! count = %zu\n", count);
		return -EINVAL;
	}

	ctrl = g_tran_ts.controller;
	if (copy_from_user(kbuf, buf, 1)) {
		tran_err("copy_from_user fail.\n");
		return -1;
	}

	ret = sscanf(kbuf, "%d", &tmp);
	if (ret != 1)
		tran_err("Invalid value! ret = %d\n", ret);

	if (tmp >= 2)
		tran_err("Invalid value! tmp = %d\n", tmp);

	val = (u8)tmp;
	g_tran_ts.game_state = val;
	tran_info("tp_game_state_write val = %d\n", val);

	if (ctrl && ctrl->ops && ctrl->ops->set_ts_mode)
		ctrl->ops->set_ts_mode(TRAN_MODE_GAME_STATE, val);

	return count;
}

static const struct proc_ops tp_game_state_fops = {
	.proc_read = tp_game_state_read,
	.proc_write = tp_game_state_write,
};

static ssize_t edge_reject_switch_proc_read(struct file *file,
					    char __user *buf,
					    size_t count, loff_t *ppos)
{
	static bool finished;
	char *kbuf;
	ssize_t len;

	tran_info("++\n");

	if (finished) {
		tran_info("read END\n");
		finished = false;
		return 0;
	}

	finished = true;
	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	len = sprintf(kbuf, "%d\n", g_tran_ts.edge_reject_switch);
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	if (copy_to_user(buf, kbuf, len)) {
		kfree(kbuf);
		return len;
	}

	*ppos += len;
	kfree(kbuf);
	tran_info("--\n");
	return len;
}

static ssize_t edge_reject_switch_proc_write(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char kbuf[3] = { 0 };
	int tmp = 0;
	int ret;

	tran_func_in();

	if (count < 1 || count > 2) {
		tran_err("Invalid value! count = %zu\n", count);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, buf, count)) {
		tran_err("copy_from_user fail.\n");
		return -1;
	}

	ret = sscanf(kbuf, "%d", &tmp);
	if (ret != 1) {
		tran_err("Invalid value! ret = %d\n", ret);
		return -EINVAL;
	}

	if (tmp >= 5) {
		tran_err("Invalid value! tmp = %d\n", tmp);
		return -EINVAL;
	}

	g_tran_ts.edge_reject_switch = tmp & 0xff;

	if (g_tran_ts.state == TRAN_SUSPEND_MODE) {
		tran_info("tp in sleep mode cancel this opration\n");
		return count;
	}

	if (ctrl && ctrl->ops && ctrl->ops->set_ts_mode)
		ctrl->ops->set_ts_mode(TRAN_MODE_EDGE_REJECT,
				       (unsigned int)(tmp & 0xff));

	return count;
}

static const struct proc_ops edge_reject_switch_fops = {
	.proc_read = edge_reject_switch_proc_read,
	.proc_write = edge_reject_switch_proc_write,
};

static int seq_file_demo_show(struct seq_file *m, void *v)
{
	char *data = m->private;
	size_t len;

	if (!data) {
		tran_info("selftest data is NULL;\n");
		return 0;
	}

	len = strlen(data);
	seq_printf(m, "%s\n", data);

	if (m->size >= len) {
		tran_info("cp fac test data end free test buf\n");
		vfree(data);
	}

	return 0;
}

static int tran_selftest_data_open(struct inode *inode, struct file *file)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char *buf;

	buf = vmalloc(SELFTEST_DATA_BUF_SIZE);
	if (!buf) {
		tran_err("selftest_data vmalloc fail\n");
		return 0;
	}

	memset(buf, 0, SELFTEST_DATA_BUF_SIZE);
	ctrl->selftest_data = buf;

	if (ctrl->ops && ctrl->ops->get_selftest_data)
		ctrl->ops->get_selftest_data(buf);

	return single_open(file, seq_file_demo_show, buf);
}

static const struct proc_ops tran_fac_selftest_fops = {
	.proc_open = tran_selftest_data_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int tran_selftest_open(struct inode *inode, struct file *file)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;

	tran_info("tran_selftest_open\n");

	if (ctrl && ctrl->ops && ctrl->ops->tran_selftest_open)
		ctrl->ops->tran_selftest_open(inode, file);

	return 0;
}

static ssize_t tran_selftest_read(struct file *file, char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char *kbuf;
	int res;
	ssize_t len;
	unsigned long ret;

	if (*ppos != 0)
		return 0;

	res = ctrl->ops->tp_selftest();
	tran_info("selftest_result:%d.\n", res);

	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (res == 1)
		len = sprintf(kbuf, "%s\n%s\n",
			      "Short Test PASS", "Open Test PASS");
	else
		len = sprintf(kbuf, "%s\n%s\n",
			      "Short Test FAIL", "Open Test FAIL");

	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	ret = copy_to_user(buf, kbuf, len);
	*ppos += len;
	kfree(kbuf);

	return ret ? (ssize_t)ret : len;
}

static int tran_selftest_close(struct inode *inode, struct file *file)
{
	tran_info("tran_selftest_close\n");
	return 0;
}

static const struct proc_ops tran_flash_fops = {
	.proc_open = tran_selftest_open,
	.proc_read = tran_selftest_read,
	.proc_release = tran_selftest_close,
};

static void tran_proc_node_create(struct tran_ts_core *ts)
{
	const char *selftest_name = "tp_selftest";

	tran_func_in();

	if (ts->platform && ts->platform->selftest_name)
		selftest_name = ts->platform->selftest_name;

	if (!proc_create(selftest_name, 0666, NULL, &tran_flash_fops))
		tran_err("create selftest proc entry failed\n");

	if (!proc_create("tran_selftest_data", 0666, NULL,
			 &tran_fac_selftest_fops))
		tran_err("create tran selftest proc entry failed\n");

	if (!proc_create("border_suppression", 0666, NULL,
			 &edge_reject_switch_fops))
		tran_err("create border_suppression proc entry failed\n");

	if (!proc_create("game_state", 0666, NULL, &tp_game_state_fops))
		tran_err("Create tp game state node fail\n");
	else
		tran_info("Create tp game state node success\n");

	if (!proc_create("tp_transient_report_rate_switch", 0666, NULL,
			 &tp_transient_report_rate_fops))
		tran_err("Create tp_transient_report_rate_switch node fail\n");
	else
		tran_info("Create tp_transient_report_rate_switch node success\n");

	if (!proc_create("tp_report_rate_switch", 0666, NULL,
			 &tp_report_rate_fops))
		tran_err("Create tp_report_rate_switch node fail\n");
	else
		tran_info("Create tp_report_rate_switch node success\n");

	if (!proc_create("game_mode", 0666, NULL, &tp_game_mode_fops))
		tran_err("Create tp game_mode node fail\n");
	else
		tran_info("Create tp game_mode node success\n");

	tran_func_out();
}

static void tran_sys_node_create(struct tran_ts_core *ts)
{
	tran_func_in();
	tran_func_out();
}

void tran_node_init(struct tran_ts_core *ts)
{
	tran_func_in();

	if (!ts->platform) {
		tran_err("tran_platform_t is NULL!\n");
		return;
	}

	tran_proc_node_create(ts);
	tran_sys_node_create(ts);
}

unsigned int tran_call_tp_cmd_handle(unsigned int cmd, void *data)
{
	struct tran_ts_controller *ctrl;

	if (!g_tran_ts.probe_done)
		return 0;

	tran_info("get ext cmd 0x%x\n", cmd);
	ctrl = g_tran_ts.controller;

	switch (cmd) {
	case TRAN_TP_CMD_RESET_LEVEL: {
		int val;

		if (!data || !ctrl || !ctrl->ext_ops ||
		    !ctrl->ext_ops->tp_reset_level)
			return 0;

		val = *(int *)data;
		if (val == 1) {
			ctrl->ext_ops->tp_reset_level(1);
			return 1;
		} else if (val == 2) {
			ctrl->ext_ops->tp_reset_level(0);
			return 1;
		}
		return 0;
	}
	case TRAN_TP_CMD_RESUME_BY_DDI:
		if (!ctrl || !ctrl->ext_ops || !ctrl->ext_ops->resume_by_ddi)
			return 0;
		ctrl->ext_ops->resume_by_ddi();
		return 1;

	case TRAN_TP_CMD_GET_GESTURE_ENABLE:
		return g_tran_ts.tpd_ops->get_gesture_status() ? 2 : 0;

	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(tran_call_tp_cmd_handle);
