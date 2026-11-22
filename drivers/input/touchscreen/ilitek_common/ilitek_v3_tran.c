// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion tran_touch integration for ILITEK TDDI driver
 */
#include "ilitek_v3.h"

static struct tran_gesture_chip_map ili_gestures_maps[] = {
	{ GESTURE_LEFT,        187 }, /* 0x62 -> 0xBB */
	{ GESTURE_UP,          186 }, /* 0x60 -> 0xBA */
	{ GESTURE_RIGHT,       170 }, /* 0x63 -> 0xAA */
	{ GESTURE_DOWN,        171 }, /* 0x61 -> 0xAB */
	{ GESTURE_DOUBLECLICK, 204 }, /* 0x58 -> 0xCC */
	{ GESTURE_W,           119 }, /* 0x65 -> 'w' */
	{ GESTURE_O,           111 }, /* 0x69 -> 'o' */
	{ GESTURE_M,           109 }, /* 0x64 -> 'm' */
	{ GESTURE_E,           101 }, /* 0x67 -> 'e' */
	{ GESTURE_C,            99 }, /* 0x66 -> 'c' */
	{ GESTURE_Z,           122 }, /* 0x6B -> 'z' */
	{ GESTURE_S,           115 }, /* 0x6A -> 's' */
	{ GESTURE_V,           118 }, /* 0x68 -> 'v' */
	{ 0, 0 },
};

static const char *support_list[] = {
	"ili77600a",
	"ili7807s",
	NULL,
};

static int ili_fwupg_get_ver_in_tp(u8 *ver)
{
	if (ili_ic_get_fw_ver() < 0) {
		ILI_ERR("Failed to get fw version\n");
		return -EINVAL;
	}
	*ver = (ilits->chip->fw_ver >> 8) & 0xFF;
	return 0;
}

u8 ili_fwupg_get_ver_in_tp_call_transsion(void)
{
	u8 ver = 0;

	if (ili_fwupg_get_ver_in_tp(&ver) < 0) {
		ILI_ERR("Failed to get fw version in tp\n");
		return 0;
	}
	ILI_INFO("fw version = 0x%x\n", ver);
	return ver;
}

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_tpd_ops **ops)
{
	if (!controller || !controller->ts) {
		tran_info("get ops fail !\n");
		return 0;
	}
	*ops = controller->ts->tpd_ops;
	if (!*ops) {
		tran_info("get ops fail !\n");
		return 0;
	}
	return 1;
}

static void tp_reset_level(int level)
{
	ilitek_reset_output(level);
}

static void resume_by_ddi(void)
{
	ili_resume_by_ddi();
}

static int ili_ts_spi_init(void)
{
	int ret;

	tran_func_in();
	ret = ilitek_commmon_init();
	if (ret < 0)
		tran_err("ilitek_commmon_init failed, ret = %d\n", ret);
	tran_func_out();
	return ret;
}

static void suspend(struct device *dev)
{
	ili_ts_suspend(dev);
}

static void resume(struct device *dev)
{
	/* Resume is handled via DDI callback resume_by_ddi() */
	(void)dev;
}

static void set_ts_mode(unsigned int mode, unsigned int val)
{
	int ret;

	switch (mode) {
	case TRAN_MODE_CHARGER:
	case TRAN_MODE_PROXIMITY:
		break;
	case TRAN_MODE_GAME_STATE:
		ret = ili_ic_func_ctrl("game_mode", val ? 0 : 1);
		if (ret < 0)
			tran_err("set game_mode failed\n");
		break;
	case TRAN_MODE_EDGE_REJECT:
		tran_info("set edge reject switch: %d\n", val);
		ili_ic_func_ctrl("edge_palm", val);
		break;
	default:
		tran_info("can't find cmd=0x%x\n", mode);
		break;
	}
}

static u8 get_firmware_version(void)
{
	tran_func_in();
	return ili_fwupg_get_ver_in_tp_call_transsion();
}

static u8 get_base_version(void)
{
	tran_func_in();
	return (u8)ilits->fw_base_ver;
}

static int tp_selftest(void)
{
	int ret;

	tran_func_in();
	ret = tran_get_nvt_selftest_result();
	tran_info("ili_test_result:%d.\n", ret);
	tran_func_out();
	return ret;
}

static int get_selftest_data(char *buf)
{
	tran_func_in();
	(void)buf;
	return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
	tran_func_in();
	(void)inode;
	(void)file;
	return 0;
}

static struct tran_ts_controller_ops controller_ops = {
	.init = ili_ts_spi_init,
	.suspend = suspend,
	.resume = resume,
	.set_ts_mode = set_ts_mode,
	.get_firmware_version = get_firmware_version,
	.get_base_version = get_base_version,
	.get_cfg_version = NULL,
	.tp_selftest = tp_selftest,
	.get_selftest_data = get_selftest_data,
	.tran_selftest_open = tran_selftest_open,
};

static struct tran_ts_controller_ext_ops controller_ext_ops = {
	.tp_reset_level = tp_reset_level,
	.resume_by_ddi = resume_by_ddi,
};

struct tran_ts_controller ili7807_controller = {
	.ic_name = NULL,
	.vendor = "ilitek",
	.gesture_maps = ili_gestures_maps,
	.selftest_data = NULL,
	.support_list = support_list,
	.input_dev = NULL,
	.ops = &controller_ops,
	.ext_ops = &controller_ext_ops,
	.ts = NULL,
};

static int __init ilitek_tran_init(void)
{
	tran_func_in();
	register_ts_controller(&ili7807_controller);
	tran_func_out();
	return 0;
}

static void __exit ilitek_tran_exit(void)
{
	tran_func_in();
	unregister_ts_controller(&ili7807_controller);
	tran_func_out();
}

module_init(ilitek_tran_init);
module_exit(ilitek_tran_exit);

MODULE_AUTHOR("ILITEK Driver Team");
MODULE_DESCRIPTION("ILITEK Touchscreen Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
