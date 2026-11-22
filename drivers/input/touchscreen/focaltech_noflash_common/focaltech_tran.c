// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech Transsion tran_touch integration & SPI driver entry
 */
#include "focaltech_core.h"

u8 fts_fwupg_get_ver_in_tp_call_transsion(void)
{
	int ret = 0;
	u8 reg_val = 0;

	ret = fts_read_reg(FTS_REG_FW_VER, &reg_val);
	if (ret < 0) {
		FTS_ERROR("read fw ver from tp fail");
		FTS_ERROR("get fw ver in tp fail");
		return 0;
	}

	FTS_INFO("fw ver in tp:0x%02x", reg_val);
	return reg_val;
}

int get_tpd_ops(struct tran_ts_controller *controller, struct tran_tpd_ops **ops)
{
	if (controller && controller->ts && controller->ts->tpd_ops) {
		*ops = controller->ts->tpd_ops;
		return 1;
	}

	tran_info("get ops fail !\n");
	return 0;
}

static void tp_reset_level(int level)
{
	tran_func_in();
}

static int fts_ts_probe(struct spi_device *spi);
static void fts_ts_remove(struct spi_device *spi);

static const struct spi_device_id fts_ts_id[] = {
	{ FTS_DRIVER_NAME, 0 },
	{ },
};

static const struct of_device_id fts_dt_match[] = {
	{ .compatible = "focaltech,fts", },
	{ },
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_spi_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int fts_ts_init(void)
{
	int ret = 0;

	tran_func_in();
	FTS_FUNC_ENTER();
	ret = spi_register_driver(&fts_ts_spi_driver);
	if (ret != 0)
		FTS_ERROR("Focaltech touch screen driver init failed!");
	FTS_FUNC_EXIT();
	tran_func_out();
	return 0;
}

static void suspend(struct device *dev)
{
	fts_ts_suspend(dev);
}

static void resume(struct device *dev)
{
	fts_ts_resume(dev);
}

static void set_ts_mode(unsigned int mode, unsigned int val)
{
	u8 cmd[2] = { 0 };

	switch (mode) {
	case TRAN_MODE_CHARGER:
		cmd[0] = FTS_REG_CHARGER_MODE_EN;
		cmd[1] = val ? 1 : 0;
		fts_write(cmd, 2);
		break;
	case TRAN_MODE_PROXIMITY:
		break;
	case TRAN_MODE_GAME_STATE:
		cmd[0] = FTS_REG_GAME_MODE_EN;
		cmd[1] = val ? 1 : 0;
		fts_write(cmd, 2);
		break;
	case TRAN_MODE_EDGE_REJECT:
		mutex_lock(&fts_data->input_dev->mutex);
		fts_set_edge_reject_switch((u8)val);
		mutex_unlock(&fts_data->input_dev->mutex);
		break;
	default:
		tran_info("can't find cmd=0x%x\n", mode);
		break;
	}
}

static u8 get_firmware_version(void)
{
	tran_func_in();
	return fts_fwupg_get_ver_in_tp_call_transsion();
}

static u8 get_base_version(void)
{
	tran_func_in();
	return fts_data->fw_ver;
}

static int tp_selftest(void)
{
	int ret = 0;

	tran_func_in();
	ret = tran_fts_get_SelftestResult();
	tran_func_out();
	return ret;
}

static int get_selftest_data(char *buf)
{
	tran_func_in();
	return 0;
}

static int tran_selftest_open(struct inode *inode, struct file *file)
{
	tran_func_in();
	return 0;
}

static const struct tran_gesture_chip_map fts_gestures_maps[] = {
	{ 0x20, 0xbb },
	{ 0x22, 0xba },
	{ 0x21, 0xaa },
	{ 0x23, 0xab },
	{ 0x24, 0xcc },
	{ 0x31, 0x77 },
	{ 0x30, 0x6f },
	{ 0x32, 0x6d },
	{ 0x33, 0x65 },
	{ 0x34, 0x63 },
	{ 0x65, 0x7a },
	{ 0x46, 0x73 },
	{ 0x54, 0x76 },
	{ 0x00, 0x00 },
};

static const char *support_list[] = {
	"ft8725",
	NULL,
};

static struct tran_ts_controller_ops controller_ops = {
	.init = fts_ts_init,
	.suspend = suspend,
	.resume = resume,
	.set_ts_mode = set_ts_mode,
	.get_firmware_version = get_firmware_version,
	.get_base_version = get_base_version,
	.get_cfg_version = NULL,
	.tp_selftest = tp_selftest,
	.get_selftest_data = get_selftest_data,
	.tran_selftest_open = tran_selftest_open,
	.reserved = NULL,
};

static struct tran_ts_controller_ext_ops controller_ext_ops = {
	.tp_reset_level = tp_reset_level,
	.resume_by_ddi = NULL,
};

struct tran_ts_controller fts_noflash_controller = {
	.ic_name = NULL,
	.vendor = "focaltech",
	.gesture_maps = fts_gestures_maps,
	.selftest_data = NULL,
	.support_list = support_list,
	.input_dev = NULL,
	.ops = &controller_ops,
	.ext_ops = &controller_ext_ops,
	.ts = NULL,
};

static int fts_ts_probe(struct spi_device *spi)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;
	struct tran_tpd_ops *ops = NULL;

	FTS_INFO("Touch Screen(SPI BUS) driver probe...");
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 6000000;
	ret = spi_setup(spi);
	if (ret < 0) {
		FTS_ERROR("spi setup fail");
		return ret;
	}

	ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	ts_data->spi = spi;
	ts_data->dev = &spi->dev;
	ts_data->log_level = 1;
	ts_data->bus_type = BUS_TYPE_SPI_V2;
	ts_data->bus_ver = 2;
	ts_data->dummy_byte = 3;
	spi_set_drvdata(spi, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
		spi_set_drvdata(spi, NULL);
		kfree(ts_data);
		return ret;
	}

	if (get_tpd_ops(&fts_noflash_controller, &ops) && ops->controller_init_finish)
		ops->controller_init_finish();

	FTS_INFO("Touch Screen(SPI BUS) driver probe successfully");
	return 0;
}

static void fts_ts_remove(struct spi_device *spi)
{
	struct fts_ts_data *ts_data = spi_get_drvdata(spi);

	FTS_FUNC_ENTER();
	if (ts_data) {
		fts_ts_remove_entry(ts_data);
		spi_set_drvdata(spi, NULL);
		kfree(ts_data);
	}
	FTS_FUNC_EXIT();
}

static int __init fts_device_init(void)
{
	FTS_INFO("++++");
	register_ts_controller(&fts_noflash_controller);
	FTS_INFO("----");
	return 0;
}

static void __exit fts_device_exit(void)
{
	FTS_INFO("++++");
	unregister_ts_controller(&fts_noflash_controller);
	spi_unregister_driver(&fts_ts_spi_driver);
	FTS_INFO("----");
}

module_init(fts_device_init);
module_exit(fts_device_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver(SPI)");
MODULE_LICENSE("GPL v2");
