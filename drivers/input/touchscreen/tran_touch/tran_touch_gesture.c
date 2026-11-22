// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion touch panel gesture subsystem (X6837 / MT6789)
 */

#include <linux/device.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

#include "tran_touch_core.h"

#define KEY_GESTURE_F13		183 /* 0xb7 */
#define KEY_GESTURE_SINGLE_TAP	190 /* 0xbe */
#define KEY_GESTURE_FOD		195 /* 0xc3 */

struct tran_gesture_data_buf {
	u8 header[10];
	u8 data[4];
	u8 extra[335];
};

static struct mutex gesture_data_mutex;
static struct tran_gesture_data_buf gesture_data;
static int gesturedouble_enabled;
static int gesture_status;
static int gesturemusic_enabled;
static int gesturechar_enabled;
static int gestureclick_enabled;
static u8 g_gtp_gesture;

struct tran_gesture_map gesture_maps[17] = {
	{ { 'a', 'l' }, 0, 0, 0xff, { 0 }, TRAN_GESTURE_CHAR_ALL },
	{ { 'c', 'c' }, 0, 0, 0xcc, { 0 }, TRAN_GESTURE_DOUBLE },
	{ { 'a', 'a' }, 0, 0, 0xaa, { 0 }, TRAN_GESTURE_MUSIC },
	{ { 'b', 'b' }, 0, 0, 0xbb, { 0 }, TRAN_GESTURE_MUSIC },
	{ { 'a', 'b' }, 0, 0, 0xab, { 0 }, TRAN_GESTURE_MUSIC },
	{ { 'b', 'a' }, 0, 0, 0xba, { 0 }, TRAN_GESTURE_MUSIC },
	{ { '5', 'e' }, 0, 0, '^',  { 0 }, TRAN_GESTURE_CLICK },
	{ { '3', 'e' }, 0, 0, '>',  { 0 }, TRAN_GESTURE_CLICK },
	{ { 'f', 'p' }, 0, 0, 0xbe, { 0 }, TRAN_GESTURE_CLICK },
	{ { 'w', '0' }, 0, 0, 'w',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'o', '0' }, 0, 0, 'o',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'm', '0' }, 0, 0, 'm',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'e', '0' }, 0, 0, 'e',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'c', '0' }, 0, 0, 'c',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'z', '0' }, 0, 0, 'z',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 's', '0' }, 0, 0, 's',  { 0 }, TRAN_GESTURE_CHAR },
	{ { 'v', '0' }, 0, 0, 'v',  { 0 }, TRAN_GESTURE_CHAR },
};

int is_all_dead(u8 *data, int len)
{
	int i;
	u8 sum = 0;

	for (i = 0; i < len; i++)
		sum |= data[i];

	return (sum == 0) ? 1 : 0;
}

void gesture_clear_data(void)
{
	mutex_lock(&gesture_data_mutex);
	memset(gesture_data.data, 0, sizeof(gesture_data.data));
	mutex_unlock(&gesture_data_mutex);
}

void gesture_deinit_node(void)
{
	remove_proc_entry("gesture_state", NULL);
	remove_proc_entry("gesture_function", NULL);
}

static int GestureHandle(void)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	struct input_dev *idev;
	u8 code;
	int i;

	if (!ctrl || !ctrl->input_dev) {
		tran_err("not find input_dev!\n");
		return -1;
	}

	idev = ctrl->input_dev;
	code = g_gtp_gesture;

	if (code == 0xbe && gestureclick_enabled) {
		tran_info("Single finger down\n");
		input_report_key(idev, KEY_GESTURE_SINGLE_TAP, 1);
		input_sync(idev);
		input_report_key(idev, KEY_GESTURE_SINGLE_TAP, 0);
		input_sync(idev);
		return 2;
	}

	if (code == 0x46 || code == 0x55) {
		if (code == 0x46 && g_tran_ts.fod_down == 0) {
			tran_info("gesture FOD key finger down\n");
			input_report_key(idev, KEY_GESTURE_FOD, 1);
			input_sync(idev);
			g_tran_ts.fod_down = 1;
			return 0;
		}
		if (code == 0x55 && g_tran_ts.fod_down == 1) {
			tran_info("gesture FOD key finger up\n");
			input_report_key(idev, KEY_GESTURE_FOD, 0);
			input_sync(idev);
			g_tran_ts.fod_down = 0;
			return 0;
		}
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(gesture_maps); i++) {
		if (gesture_maps[i].code == code) {
			if (gesture_maps[i].type == TRAN_GESTURE_CHAR &&
			    gesturechar_enabled != 1)
				break;
			if (!gesture_maps[i].enabled)
				break;

			gesture_data.data[0] = code;
			gesture_data.data[1] = 0;
			gesture_data.data[2] = 0;
			gesture_data.data[3] = 0;

			input_report_key(idev, KEY_GESTURE_F13, 1);
			input_sync(idev);
			input_report_key(idev, KEY_GESTURE_F13, 0);
			input_sync(idev);

			tran_info("[GESTURE]gesture report KEY_F13 done !\n");
			return 0;
		}
	}

	tran_info("[GESTURE]Sorry, this gesture has been disabled.\n");
	return 1;
}

static int GestureIdTranslate(int raw_id)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	const struct tran_gesture_chip_map *maps;
	int i = 0;

	if (!ctrl || !ctrl->gesture_maps) {
		g_gtp_gesture = 0xff;
		return -1;
	}

	maps = ctrl->gesture_maps;
	while (maps[i].tran_id != 0) {
		if (maps[i].chip_id == (u8)raw_id) {
			g_gtp_gesture = maps[i].tran_id;
			if (g_gtp_gesture != 0x46)
				tran_info("[GESTURE]g_gtp_gesture=0x%x\n",
					  g_gtp_gesture);
			return GestureHandle();
		}
		i++;
	}

	g_gtp_gesture = 0xff;
	return -1;
}

static int SetDozeStatus(void)
{
	if (gesturedouble_enabled || gesturechar_enabled ||
	    gesturemusic_enabled || gestureclick_enabled)
		gesture_status = 1;
	else
		gesture_status = 0;

	return gesture_status;
}

static int gesture_switch_status(int type)
{
	switch (type) {
	case TRAN_GESTURE_DOUBLE:
		return gesturedouble_enabled;
	case TRAN_GESTURE_MUSIC:
		return gesturemusic_enabled;
	case TRAN_GESTURE_CHAR_ALL:
		return gesturechar_enabled;
	case TRAN_GESTURE_CLICK:
		return gestureclick_enabled;
	default:
		return 0;
	}
}

static ssize_t show_gesture_test(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gesture_status);
}

static ssize_t store_gesture_test(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	if (buf[0] == '1') {
		gesturedouble_enabled = 1;
		tran_info("[GESTURE]gesture test on\n");
	} else if (buf[0] == '0') {
		gesturedouble_enabled = 0;
		tran_info("[GESTURE]gesture test off\n");
	}

	return count;
}

static DEVICE_ATTR(gesture_test, 0664, show_gesture_test, store_gesture_test);

static struct attribute *gesture_attributes[] = {
	&dev_attr_gesture_test.attr,
	NULL,
};

static struct attribute_group gesture_attribute_group = {
	.attrs = gesture_attributes,
};

static ssize_t gesture_status_read(struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	char *kbuf;
	char *p;
	int i;
	ssize_t len;
	unsigned long ret;

	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	p = kbuf;
	for (i = 0; i < ARRAY_SIZE(gesture_maps); i++) {
		p += sprintf(p, "%c%c : %d\n",
			     gesture_maps[i].name[0],
			     gesture_maps[i].name[1],
			     gesture_maps[i].enabled);
	}

	len = p - kbuf;
	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	ret = copy_to_user(buf, kbuf, len);
	*ppos += len;
	kfree(kbuf);

	return ret ? (ssize_t)ret : len;
}

static ssize_t gesture_status_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	return 0;
}

static const struct proc_ops gesture_state_ops = {
	.proc_read = gesture_status_read,
	.proc_write = gesture_status_write,
};

static ssize_t gesture_data_read(struct file *file, char __user *buf,
				 size_t count, loff_t *ppos)
{
	char *kbuf;
	u8 ges;
	char ch;
	ssize_t len;
	unsigned long ret;

	kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	ges = gesture_data.data[0];
	ch = (ges == 0xcc) ? 'f' : (char)ges;
	len = sprintf(kbuf, "%c\n", ch);

	if (*ppos >= len) {
		kfree(kbuf);
		return 0;
	}

	ret = copy_to_user(buf, kbuf, len);
	*ppos += len;
	tran_info("CTP gesture ret:%d ppos:%d ges:%x char:%c\n",
		  (int)ret, (int)*ppos, gesture_data.data[0], kbuf[0]);
	kfree(kbuf);

	return ret ? (ssize_t)ret : len;
}

static ssize_t gesture_data_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	char cmd[4] = "000";
	int enable;
	struct tran_gesture_map *matched = NULL;
	int i;

	if (copy_from_user(cmd, buf, 3)) {
		tran_err(" copy_from_user failed.\n");
		return -1;
	}

	if (cmd[2] == '1')
		enable = 1;
	else if (cmd[2] == '2')
		enable = 0;
	else
		enable = 2;

	for (i = 0; i < ARRAY_SIZE(gesture_maps); i++) {
		if (strncmp(cmd, gesture_maps[i].name, 2) == 0)
			matched = &gesture_maps[i];
	}

	if (!matched || enable == 2 || matched->code == 0) {
		tran_err(" bad command format 0:%c 1:%c 2:%c!!!\n",
			 cmd[0], cmd[1], cmd[2]);
		return -1;
	}

	tran_info("gesture_data_write code1[%x] enable[%d]\n",
		  matched->code, enable);

	switch (matched->type) {
	case TRAN_GESTURE_DOUBLE:
		gesturedouble_enabled = enable;
		break;
	case TRAN_GESTURE_MUSIC:
		gesturemusic_enabled = enable;
		break;
	case TRAN_GESTURE_CHAR_ALL:
		gesturechar_enabled = enable;
		break;
	case TRAN_GESTURE_CLICK:
		gestureclick_enabled = enable;
		break;
	default:
		break;
	}

	matched->enabled = (enable != 0) ? 1 : 0;
	return count;
}

static const struct proc_ops gesture_node_ops = {
	.proc_read = gesture_data_read,
	.proc_write = gesture_data_write,
};

void gesture_init(struct tran_ts_core *ts)
{
	struct proc_dir_entry *entry;
	int ret;

	tran_func_in();

	mutex_init(&gesture_data_mutex);
	memset(&gesture_data, 0, sizeof(gesture_data));

	ts->tpd_ops->gesture_id_translate = GestureIdTranslate;
	ts->tpd_ops->get_gesture_status = SetDozeStatus;
	ts->tpd_ops->gesture_switch_status = gesture_switch_status;

	entry = proc_create("gesture_function", 0666, NULL, &gesture_node_ops);
	if (!entry) {
		tran_err("[GESTURE]Couldn't create proc entry[GESTURE_NODE]!\n");
		return;
	}

	entry = proc_create("gesture_state", 0666, NULL, &gesture_state_ops);
	if (!entry) {
		tran_err("[GESTURE]Couldn't create proc entry[gesture_state]!\n");
		return;
	}

	ret = sysfs_create_group(&ts->pdev->dev.kobj, &gesture_attribute_group);
	if (ret) {
		tran_err("sysfs_create_group() gesture failed!!\n");
		sysfs_remove_group(&ts->pdev->dev.kobj, &gesture_attribute_group);
		return;
	}

	tran_info("sysfs_create_group() gesture succeeded!!\n");
	tran_func_out();
}
