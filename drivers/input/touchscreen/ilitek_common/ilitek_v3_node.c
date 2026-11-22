// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI procfs (/proc/ilitek/) and IOCTL interface
 */
#include "ilitek_v3.h"

#define USER_STR_BUFF			4096
#define IOCTL_I2C_BUFF			PAGE_SIZE

struct proc_file_entry {
	const char *name;
	struct proc_dir_entry *node;
	const struct proc_ops *fops;
	bool isCreated;
};

struct rw_tp_reg_data {
	u32 type;
	u32 addr;
	u32 data;
	u32 wlen;
	u32 rlen;
	u32 mcu;
};

static char g_user_buf[USER_STR_BUFF];
static struct proc_dir_entry *proc_dir_ilitek;
static u8 fw_cmd_buff[256];
static struct rw_tp_reg_data rw_reg;

void ili_str2hex(char *str)
{
	int strlen_val, result, intermed, intermedtop;
	char *s = str;

	while (*s != 0x0)
		s++;

	strlen_val = (int)(s - str);
	s = str;
	if (*s != 0x30)
		return;

	s++;
	if (*s != 0x78 && *s != 0x58)
		return;
	s++;

	strlen_val = strlen_val - 3;
	result = 0;
	while (*s != 0x0) {
		intermed = *s & 0x0F;
		intermedtop = *s & 0xF0;
		if (intermedtop == 0x60 || intermedtop == 0x40)
			intermed += 0x09;
		intermed = intermed << (strlen_val << 2);
		result = result | intermed;
		strlen_val -= 1;
		s++;
	}
	snprintf(str, 16, "%d", result);
}

int ili_katoi(char *str)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*str == '-') {
		sign = 1;
		str += 1;
	} else {
		sign = 0;
		if (*str == '+')
			str += 1;
	}

	for (;; str += 1) {
		digit = *str - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign)
		return -result;
	return result;
}

int file_write(struct file *file, loff_t *pos, const char *data, size_t len)
{
	/* Stub in GKI: kernel file writes disabled */
	(void)file;
	(void)pos;
	(void)data;
	(void)len;
	return 0;
}

int ili_cascade_rw_tp_reg(u32 mcu, u32 type, u32 addr, u32 wdata, u32 wlen, u32 rlen)
{
	int ret = 0;
	u32 rdata = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice)
		ili_ice_mode_ctrl(ENABLE, mcu ? true : false);

	if (type == 0) {
		ret = ili_ice_mode_read(addr, &rdata, rlen ? rlen : 4);
		ILI_INFO("Read reg 0x%x = 0x%x\n", addr, rdata);
		rw_reg.data = rdata;
	} else {
		ret = ili_ice_mode_write(addr, wdata, wlen ? wlen : 4);
		ILI_INFO("Write reg 0x%x = 0x%x\n", addr, wdata);
	}

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, mcu ? true : false);

	return ret;
}

int ili_get_tp_recore_ctrl(int data)
{
	return ili_ic_func_ctrl("tp_recore", data);
}

int ili_get_tp_recore_data(bool mcu)
{
	ILI_INFO("get tp recore data (mcu = %d)\n", mcu);
	return 0;
}

void ili_gesture_fail_reason(bool enable)
{
	u8 cmd[24] = { 0 };

	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x10;
	cmd[3] = enable ? 0x01 : 0x00;
	ilits->wrapper(cmd, 4, NULL, 0, true, false);
}

int ili_tp_data_mode_ctrl(u8 *cmd)
{
	int ret = 0;

	if (!cmd)
		return -EINVAL;

	switch (cmd[0]) {
	case P5_X_FW_AP_MODE:
		ret = ili_set_tp_data_len(DATA_FORMAT_DEMO, true, NULL);
		break;
	case P5_X_FW_TEST_MODE:
		ret = ili_switch_tp_mode(P5_X_FW_TEST_MODE);
		break;
	default:
		ret = ili_set_tp_data_len(cmd[0], true, NULL);
		break;
	}
	return ret;
}

int ili_mp_lcm_off_env_ctrl(int mode)
{
	ILI_INFO("mp lcm off env ctrl: %d\n", mode);
	return 0;
}

int tran_get_nvt_selftest_result(void)
{
	int ret;

	ILI_INFO("Start transsion MP selftest\n");
	memset(g_user_buf, 0, USER_STR_BUFF);
	ret = ili_mp_test_handler(g_user_buf, true);
	ILI_INFO("MP TEST %s, Error code = %d\n", ret < 0 ? "FAIL" : "PASS", ret);
	return (ret < 0) ? -1 : 1;
}

/* ---- Procfs read/write/seq/ioctl callbacks ---- */

static int ilitek_proc_mp_result_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", g_user_buf);
	return 0;
}

static int proc_show_mp_test_result_read(struct inode *inode, struct file *file)
{
	return single_open(file, ilitek_proc_mp_result_show, NULL);
}

static ssize_t ilitek_proc_fw_cmd_read(struct file *pFile, char __user *buf,
				       size_t size, loff_t *pos)
{
	int len = 0;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "fw_cmd: 0x%02x 0x%02x 0x%02x\n",
		       fw_cmd_buff[0], fw_cmd_buff[1], fw_cmd_buff[2]);
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_fw_cmd_write(struct file *filp, const char __user *buff,
					size_t size, loff_t *pos)
{
	char tmp[128] = { 0 };
	size_t n = min_t(size_t, size, sizeof(tmp) - 1);

	if (copy_from_user(tmp, buff, n))
		return -EFAULT;

	ILI_INFO("fw_cmd write: %s\n", tmp);
	return size;
}

static ssize_t ilitek_proc_sram_test_info(struct file *pFile, char __user *buf,
					  size_t size, loff_t *pos)
{
	int ret, len;

	if (*pos != 0)
		return 0;

	mutex_lock(&ilits->touch_mutex);
	ret = ili_tddi_ic_sram_test();
	mutex_unlock(&ilits->touch_mutex);

	len = snprintf(g_user_buf, USER_STR_BUFF, "SRAM TEST %s\n",
		       ret == 0 ? "PASS" : "FAIL");
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_node_change_list_read(struct file *pFile, char __user *buf,
					    size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "Driver version: %s\n", DRIVER_VERSION);
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_node_ver_info_read(struct file *pFile, char __user *buf,
					 size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF,
		       "Driver version = %s\n"
		       "IC = 0x%x\n"
		       "FW version = 0x%x\n"
		       "Protocol version = 0x%x\n"
		       "Core version = 0x%x\n",
		       DRIVER_VERSION,
		       ilits->chip ? ilits->chip->pid : 0,
		       ilits->chip ? ilits->chip->fw_ver : 0,
		       ilits->protocol ? ilits->protocol->ver : 0,
		       ilits->chip ? ilits->chip->core_ver : 0);
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_rw_tp_reg_read(struct file *pFile, char __user *buf,
					  size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "addr=0x%x, data=0x%x\n",
		       rw_reg.addr, rw_reg.data);
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_rw_tp_reg_write(struct file *filp, const char __user *buff,
					   size_t size, loff_t *pos)
{
	char tmp[128] = { 0 };
	size_t n = min_t(size_t, size, sizeof(tmp) - 1);

	if (copy_from_user(tmp, buff, n))
		return -EFAULT;

	ILI_INFO("rw_tp_reg write: %s\n", tmp);
	return size;
}

static ssize_t ilitek_proc_get_debug_mode_data_read(struct file *filp, char __user *buff,
						    size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "debug_mode_data ready\n");
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_get_debug_mode_data_write(struct file *filp, const char __user *buff,
						     size_t size, loff_t *pos)
{
	char tmp[64] = { 0 };
	size_t n = min_t(size_t, size, sizeof(tmp) - 1);

	if (copy_from_user(tmp, buff, n))
		return -EFAULT;

	return size;
}

static ssize_t ilitek_proc_fw_get_raw_data_read(struct file *pFile, char __user *buf,
						size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "=======Raw data=======\n");
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_get_delta_data_read(struct file *pFile, char __user *buf,
					       size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "=======Delta data=======\n");
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_fw_pc_counter_read(struct file *pFile, char __user *buf,
					      size_t size, loff_t *pos)
{
	u32 pc;
	int len;

	if (*pos != 0)
		return 0;

	pc = ili_ic_get_pc_counter(0);
	len = snprintf(g_user_buf, USER_STR_BUFF, "pc = 0x%x\n", pc);
	if (copy_to_user(buf, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_debug_all_switch_read(struct file *pFile, char __user *buff,
						 size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	ilits->dnp = !ilits->dnp;
	len = snprintf(g_user_buf, USER_STR_BUFF, "dlnp : %s\n",
		       ilits->dnp ? "Enabled" : "Disabled");
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static int ilitek_proc_debug_all_message_show(struct seq_file *m, void *v)
{
	seq_printf(m, "dbf_in_idx=%d, dbf_out_idx=%d, ppb=%d\n",
		   ilits->dbf_in_idx, ilits->dbf_out_idx, ilits->ppb);
	return 0;
}

static int ilitek_proc_seq_debug_all_message_read(struct inode *inode, struct file *file)
{
	return single_open(file, ilitek_proc_debug_all_message_show, NULL);
}

static ssize_t ilitek_proc_debug_switch_read(struct file *pFile, char __user *buff,
					     size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	ilits->dnp = !ilits->dnp;
	len = snprintf(g_user_buf, USER_STR_BUFF, "dnp : %s\n",
		       ilits->dnp ? "Enabled" : "Disabled");
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static int ilitek_proc_debug_message_show(struct seq_file *m, void *v)
{
	seq_printf(m, "dnp=%d\n", ilits->dnp);
	return 0;
}

static int ilitek_proc_seq_debug_message_read(struct inode *inode, struct file *file)
{
	return single_open(file, ilitek_proc_debug_message_show, NULL);
}

static ssize_t ilitek_node_mp_lcm_off_test_read(struct file *filp, char __user *buff,
						size_t size, loff_t *pos)
{
	int ret, len;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF);
	ret = ili_mp_test_handler(g_user_buf, false);
	len = snprintf(g_user_buf, USER_STR_BUFF, "MP LCM OFF: %s (%d)\n",
		       ret < 0 ? "FAIL" : "PASS", ret);
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_node_mp_lcm_on_test_read(struct file *filp, char __user *buff,
					       size_t size, loff_t *pos)
{
	int ret, len;

	if (*pos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF);
	ret = ili_mp_test_handler(g_user_buf, true);
	len = snprintf(g_user_buf, USER_STR_BUFF, "MP LCM ON: %s (%d)\n",
		       ret < 0 ? "FAIL" : "PASS", ret);
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_debug_level_read(struct file *filp, char __user *buff,
					    size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	debug_en = !debug_en;
	len = snprintf(g_user_buf, USER_STR_BUFF, "debug_en = %d\n", debug_en);
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_node_fw_upgrade_read(struct file *filp, char __user *buff,
					   size_t size, loff_t *pos)
{
	int ret, len;

	if (*pos != 0)
		return 0;

	mutex_lock(&ilits->touch_mutex);
	ilits->node_update = true;
	ret = ili_fw_upgrade_handler(NULL);
	ilits->node_update = false;
	mutex_unlock(&ilits->touch_mutex);

	len = snprintf(g_user_buf, USER_STR_BUFF, "FW upgrade %s\n",
		       ret == 0 ? "PASS" : "FAIL");
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_proc_fw_process_read(struct file *filp, char __user *buff,
					   size_t size, loff_t *pos)
{
	int len;

	if (*pos != 0)
		return 0;

	len = snprintf(g_user_buf, USER_STR_BUFF, "%d\n", ilits->fw_update_stat);
	if (copy_to_user(buff, g_user_buf, len))
		return -EFAULT;
	*pos += len;
	return len;
}

static ssize_t ilitek_node_ioctl_write(struct file *filp, const char __user *buff,
				       size_t size, loff_t *pos)
{
	char cmd[256] = { 0 };
	size_t n = min_t(size_t, size, sizeof(cmd) - 1);

	if (copy_from_user(cmd, buff, n))
		return -EFAULT;

	ILI_INFO("ioctl write: %s\n", cmd);

	mutex_lock(&ilits->touch_mutex);
	if (strncmp(cmd, "releasetouch", 12) == 0) {
		ili_touch_release_all_point();
	} else if (strncmp(cmd, "disablewqesd", 12) == 0) {
		ili_wq_ctrl(WQ_ESD, DISABLE);
	} else if (strncmp(cmd, "enablewqesd", 11) == 0) {
		ili_wq_ctrl(WQ_ESD, ENABLE);
	} else if (strncmp(cmd, "gesture", 7) == 0) {
		ilits->gesture = !ilits->gesture;
		ILI_INFO("gesture = %d\n", ilits->gesture);
	} else if (strncmp(cmd, "reset", 5) == 0) {
		ili_reset_ctrl(ilits->reset);
	}
	mutex_unlock(&ilits->touch_mutex);

	return size;
}

static long ilitek_node_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	static u16 i2c_rw_length;
	int ret = 0;

	if (_IOC_TYPE(cmd) != ILITEK_IOCTL_MAGIC) {
		ILI_ERR("The Magic number doesn't match\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > ILITEK_IOCTL_MAXNR) {
		ILI_ERR("The number of ioctl doesn't match\n");
		return -ENOTTY;
	}

	mutex_lock(&ilits->touch_mutex);
	switch (_IOC_NR(cmd)) {
	case 0:
		ili_reset_ctrl(ilits->reset);
		break;
	case 1:
		ili_touch_release_all_point();
		break;
	case 2:
		if (copy_from_user(&i2c_rw_length, (u16 __user *)arg, sizeof(u16)))
			ret = -EFAULT;
		break;
	default:
		ILI_DBG("ioctl nr = %u\n", _IOC_NR(cmd));
		break;
	}
	mutex_unlock(&ilits->touch_mutex);
	return ret;
}

static long ilitek_node_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	ILI_DBG("compat_ioctl: cmd = 0x%x\n", cmd);
	return ilitek_node_ioctl(filp, cmd, arg);
}

/* ---- 20 proc_ops structs matching stock ilitek_common.ko ---- */

static const struct proc_ops proc_ioctl_fops = {
	.proc_write = ilitek_node_ioctl_write,
	.proc_lseek = default_llseek,
	.proc_ioctl = ilitek_node_ioctl,
	.proc_compat_ioctl = ilitek_node_compat_ioctl,
};

static const struct proc_ops proc_fw_process_fops = {
	.proc_read = ilitek_proc_fw_process_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_fw_upgrade_fops = {
	.proc_read = ilitek_node_fw_upgrade_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_debug_level_fops = {
	.proc_read = ilitek_proc_debug_level_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_mp_lcm_on_test_fops = {
	.proc_read = ilitek_node_mp_lcm_on_test_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_mp_lcm_off_test_fops = {
	.proc_read = ilitek_node_mp_lcm_off_test_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_debug_message_fops = {
	.proc_open = ilitek_proc_seq_debug_message_read,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_debug_message_switch_fops = {
	.proc_read = ilitek_proc_debug_switch_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_debug_all_message_fops = {
	.proc_open = ilitek_proc_seq_debug_all_message_read,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops proc_debug_all_message_switch_fops = {
	.proc_read = ilitek_proc_debug_all_switch_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_fw_pc_counter_fops = {
	.proc_read = ilitek_proc_fw_pc_counter_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_get_delta_data_fops = {
	.proc_read = ilitek_proc_get_delta_data_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_get_raw_data_fops = {
	.proc_read = ilitek_proc_fw_get_raw_data_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_get_debug_mode_data_fops = {
	.proc_read = ilitek_proc_get_debug_mode_data_read,
	.proc_write = ilitek_proc_get_debug_mode_data_write,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_rw_tp_reg_fops = {
	.proc_read = ilitek_proc_rw_tp_reg_read,
	.proc_write = ilitek_proc_rw_tp_reg_write,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_ver_info_fops = {
	.proc_read = ilitek_node_ver_info_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_change_list_fops = {
	.proc_read = ilitek_node_change_list_read,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_sram_test_fops = {
	.proc_read = ilitek_proc_sram_test_info,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_fw_cmd_fops = {
	.proc_read = ilitek_proc_fw_cmd_read,
	.proc_write = ilitek_proc_fw_cmd_write,
	.proc_lseek = default_llseek,
};

static const struct proc_ops proc_show_mp_test_result_fops = {
	.proc_open = proc_show_mp_test_result_read,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

struct proc_file_entry iliproc[] = {
	{ "ioctl",                    NULL, &proc_ioctl_fops,                    false },
	{ "fw_process",               NULL, &proc_fw_process_fops,               false },
	{ "fw_upgrade",               NULL, &proc_fw_upgrade_fops,               false },
	{ "debug_level",              NULL, &proc_debug_level_fops,              false },
	{ "mp_lcm_on_test",           NULL, &proc_mp_lcm_on_test_fops,           false },
	{ "mp_lcm_off_test",          NULL, &proc_mp_lcm_off_test_fops,          false },
	{ "debug_message",            NULL, &proc_debug_message_fops,            false },
	{ "debug_message_switch",     NULL, &proc_debug_message_switch_fops,     false },
	{ "debug_all_message",        NULL, &proc_debug_all_message_fops,        false },
	{ "debug_all_message_switch", NULL, &proc_debug_all_message_switch_fops, false },
	{ "fw_pc_counter",            NULL, &proc_fw_pc_counter_fops,            false },
	{ "show_delta_data",          NULL, &proc_get_delta_data_fops,           false },
	{ "show_raw_data",            NULL, &proc_get_raw_data_fops,             false },
	{ "get_debug_mode_data",      NULL, &proc_get_debug_mode_data_fops,      false },
	{ "rw_tp_reg",                NULL, &proc_rw_tp_reg_fops,                false },
	{ "ver_info",                 NULL, &proc_ver_info_fops,                 false },
	{ "change_list",              NULL, &proc_change_list_fops,              false },
	{ "sram_test",                NULL, &proc_sram_test_fops,                false },
	{ "fw_cmd",                   NULL, &proc_fw_cmd_fops,                   false },
	{ "show_mp_test_result",      NULL, &proc_show_mp_test_result_fops,      false },
};

void ili_node_init(void)
{
	int i;

	proc_dir_ilitek = proc_mkdir("ilitek", NULL);
	if (!proc_dir_ilitek) {
		ILI_ERR("Failed to create ilitek under /proc\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(iliproc); i++) {
		iliproc[i].node = proc_create(iliproc[i].name, 0644,
					      proc_dir_ilitek, iliproc[i].fops);
		if (!iliproc[i].node) {
			iliproc[i].isCreated = false;
			ILI_ERR("Failed to create %s under /proc\n", iliproc[i].name);
		} else {
			iliproc[i].isCreated = true;
			ILI_INFO("Succeed to create %s under /proc\n", iliproc[i].name);
		}
	}
}
