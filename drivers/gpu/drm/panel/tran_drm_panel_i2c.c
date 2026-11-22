// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * Transsion DRM panel I2C bias driver.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/types.h>

#define I2C_ID_NAME "I2C_LCD_BIAS"

static struct i2c_client *tran_panel_i2c_client;

int tran_panel_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = tran_panel_i2c_client;
	char write_data[2] = { 0 };

	if (!client) {
		pr_err("%s: ERROR!! client is null\n", __func__);
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("%s: ERROR write 0x%x with 0x%x fail %d\n",
		       __func__, addr, value, ret);

	return ret;
}
EXPORT_SYMBOL(tran_panel_i2c_write_bytes);

int tran_panel_i2c_read_bytes(unsigned char addr, unsigned char *value)
{
	int ret = 0;
	struct i2c_client *client = tran_panel_i2c_client;
	char buf[2] = { 0 };

	if (!client) {
		pr_err("%s: ERROR!! tran_panel_i2c_client is null\n", __func__);
		return 0;
	}

	buf[0] = addr;
	i2c_master_send(client, &buf[0], 1);
	ret = i2c_master_recv(client, &buf[1], 1);
	if (ret < 0)
		pr_err("%s: ERROR read 0x%x fail %d\n", __func__, addr, ret);

	*value = buf[1];
	return ret;
}
EXPORT_SYMBOL(tran_panel_i2c_read_bytes);

static int tran_panel_i2c_probe(struct i2c_client *client)
{
	pr_info("%s: name=%s addr=0x%x\n", __func__, client->name, client->addr);
	tran_panel_i2c_client = client;
	return 0;
}

static void tran_panel_i2c_remove(struct i2c_client *client)
{
	pr_info("%s: name=%s addr=0x%x\n", __func__, client->name, client->addr);
	tran_panel_i2c_client = NULL;
	i2c_unregister_device(client);
}

static const struct of_device_id tran_panel_i2c_of_match[] = {
	{ .compatible = "mediatek,i2c_lcd_bias" },
	{ }
};

static const struct i2c_device_id mtk_panel_i2c_id[] = {
	{ I2C_ID_NAME, 0 },
	{ }
};

struct i2c_driver tran_drm_i2c_driver = {
	.id_table = mtk_panel_i2c_id,
	.probe = tran_panel_i2c_probe,
	.remove = tran_panel_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = I2C_ID_NAME,
		.of_match_table = tran_panel_i2c_of_match,
	},
};

static int __init tran_drm_i2c_init(void)
{
	int ret;

	pr_info("%s, add i2c driver\n", __func__);
	ret = i2c_add_driver(&tran_drm_i2c_driver);
	if (ret < 0) {
		pr_err("%s, failed to register i2c driver: %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void __exit tran_drm_i2c_exit(void)
{
	i2c_del_driver(&tran_drm_i2c_driver);
}

module_init(tran_drm_i2c_init);
module_exit(tran_drm_i2c_exit);

MODULE_AUTHOR("you.tan@transsion.com>");
MODULE_DESCRIPTION("mediatek, panel i2c driver");
MODULE_LICENSE("GPL v2");
