// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * External camera LDO PMIC driver (ET5904/SGM38121 compatible).
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "imgsensor_hw.h"

#define EXT_LDO_REG_DISCHARGE	0x02
#define EXT_LDO_REG_DVDD1	0x03 /* cam0 DVDD (bit 0) */
#define EXT_LDO_REG_DVDD2	0x04 /* cam1 DVDD (bit 1) */
#define EXT_LDO_REG_AVDD1	0x05 /* cam0 AVDD (bit 2) */
#define EXT_LDO_REG_AVDD2	0x06 /* cam1 AVDD (bit 3) */
#define EXT_LDO_REG_ENABLE	0x0e

static struct i2c_client *ext_ldo_i2c_client;
static struct pinctrl *ext_ldo_pinctrl;
static struct pinctrl_state *ext_ldo_hwen_high;
static struct pinctrl_state *ext_ldo_hwen_low;
static DEFINE_MUTEX(ext_ldo_mutex);
static bool ext_ldo_registered;

static void x6837_ext_ldo_set_hwen(bool high)
{
	if (!ext_ldo_pinctrl)
		return;
	if (high && !IS_ERR_OR_NULL(ext_ldo_hwen_high)) {
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_high);
		usleep_range(1000, 1500);
	} else if (!high && !IS_ERR_OR_NULL(ext_ldo_hwen_low)) {
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_low);
	}
}

int x6837_ext_ldo_enable(int sensor_idx, int pin, int voltage_uv)
{
	int ret, en_val;
	u8 vreg, vval, dval, bit;

	mutex_lock(&ext_ldo_mutex);
	if (!ext_ldo_i2c_client) {
		pr_err("[x6837_ext_ldo] i2c_client is NULL (idx=%d pin=%d)\n", sensor_idx, pin);
		mutex_unlock(&ext_ldo_mutex);
		return -ENODEV;
	}

	x6837_ext_ldo_set_hwen(true);

	if (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN && pin == IMGSENSOR_HW_PIN_AVDD) {
		vreg = EXT_LDO_REG_AVDD1;
		vval = (voltage_uv > 1200000) ? ((voltage_uv - 1200000) / 12500) : 0;
		dval = 0x04;
		bit = BIT(2);
	} else if (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN && pin == IMGSENSOR_HW_PIN_DVDD) {
		vreg = EXT_LDO_REG_DVDD1;
		vval = (voltage_uv > 600000) ? ((voltage_uv - 600000) / 6000) : 0;
		dval = 0x01;
		bit = BIT(0);
	} else if (sensor_idx == IMGSENSOR_SENSOR_IDX_SUB && pin == IMGSENSOR_HW_PIN_AVDD) {
		vreg = EXT_LDO_REG_AVDD2;
		vval = (voltage_uv > 1200000) ? ((voltage_uv - 1200000) / 12500) : 0;
		dval = 0x04;
		bit = BIT(3);
	} else if (sensor_idx == IMGSENSOR_SENSOR_IDX_SUB && pin == IMGSENSOR_HW_PIN_DVDD) {
		vreg = EXT_LDO_REG_DVDD2;
		vval = (voltage_uv > 600000) ? ((voltage_uv - 600000) / 6000) : 0;
		dval = 0x02;
		bit = BIT(1);
	} else {
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	en_val = i2c_smbus_read_byte_data(ext_ldo_i2c_client, EXT_LDO_REG_ENABLE);
	if (en_val < 0) {
		pr_err("[x6837_ext_ldo] read EN failed: %d\n", en_val);
		mutex_unlock(&ext_ldo_mutex);
		return en_val;
	}

	ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, vreg, vval);
	if (ret < 0) {
		pr_err("[x6837_ext_ldo] write reg 0x%x=0x%x failed: %d\n", vreg, vval, ret);
		mutex_unlock(&ext_ldo_mutex);
		return ret;
	}
	i2c_smbus_write_byte_data(ext_ldo_i2c_client, EXT_LDO_REG_DISCHARGE, dval);
	en_val |= bit;
	ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, EXT_LDO_REG_ENABLE, en_val);
	pr_info("[x6837_ext_ldo] ON idx=%d pin=%d uV=%d -> reg0x%x=0x%x en=0x%x ret=%d\n",
		sensor_idx, pin, voltage_uv, vreg, vval, en_val, ret);
	mutex_unlock(&ext_ldo_mutex);
	return ret;
}

int x6837_ext_ldo_disable(int sensor_idx, int pin)
{
	int ret, en_val;
	u8 bit;

	mutex_lock(&ext_ldo_mutex);
	if (!ext_ldo_i2c_client) {
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	if (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN && pin == IMGSENSOR_HW_PIN_AVDD)
		bit = BIT(2);
	else if (sensor_idx == IMGSENSOR_SENSOR_IDX_MAIN && pin == IMGSENSOR_HW_PIN_DVDD)
		bit = BIT(0);
	else if (sensor_idx == IMGSENSOR_SENSOR_IDX_SUB && pin == IMGSENSOR_HW_PIN_AVDD)
		bit = BIT(3);
	else if (sensor_idx == IMGSENSOR_SENSOR_IDX_SUB && pin == IMGSENSOR_HW_PIN_DVDD)
		bit = BIT(1);
	else {
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	en_val = i2c_smbus_read_byte_data(ext_ldo_i2c_client, EXT_LDO_REG_ENABLE);
	if (en_val >= 0) {
		en_val &= ~bit;
		ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, EXT_LDO_REG_ENABLE, en_val);
	} else {
		ret = en_val;
	}
	mutex_unlock(&ext_ldo_mutex);
	return ret;
}

static int ext_ldo_pdrv_probe(struct platform_device *pdev)
{
	ext_ldo_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ext_ldo_pinctrl)) {
		dev_err(&pdev->dev, "failed to get ext_ldo pinctrl\n");
		ext_ldo_pinctrl = NULL;
		return 0;
	}
	ext_ldo_hwen_high = pinctrl_lookup_state(ext_ldo_pinctrl, "extldo_en_high");
	ext_ldo_hwen_low = pinctrl_lookup_state(ext_ldo_pinctrl, "extldo_en_low");
	x6837_ext_ldo_set_hwen(true);
	dev_info(&pdev->dev, "[x6837_ext_ldo] cam_vdd_extldo HWEN ready\n");
	return 0;
}

static const struct of_device_id ext_ldo_of_match[] = {
	{ .compatible = "mediatek,cam_vdd_extldo" },
	{},
};

static struct platform_driver ext_ldo_platform_driver = {
	.probe = ext_ldo_pdrv_probe,
	.driver = {
		.name = "cam_vdd_extldo",
		.of_match_table = ext_ldo_of_match,
	},
};

static int ext_ldo_i2c_probe(struct i2c_client *client)
{
	int id;

	ext_ldo_i2c_client = client;
	x6837_ext_ldo_set_hwen(true);
	id = i2c_smbus_read_byte_data(client, 0x00);
	dev_info(&client->dev, "[x6837_ext_ldo] cam_vdd_ext@0x%x probed, chip_id=0x%x\n",
		client->addr, id);
	return 0;
}

static const struct i2c_device_id ext_ldo_i2c_id[] = {
	{ "cam_vdd_ext", 0 },
	{},
};

static const struct of_device_id ext_ldo_i2c_of_match[] = {
	{ .compatible = "mediatek,cam_vdd_ext" },
	{},
};

static struct i2c_driver ext_ldo_i2c_driver = {
	.driver = {
		.name = "cam_vdd_ext",
		.of_match_table = ext_ldo_i2c_of_match,
	},
	.probe = ext_ldo_i2c_probe,
	.id_table = ext_ldo_i2c_id,
};

void x6837_ext_ldo_init(void)
{
	if (ext_ldo_registered)
		return;
	ext_ldo_registered = true;
	platform_driver_register(&ext_ldo_platform_driver);
	i2c_add_driver(&ext_ldo_i2c_driver);
}
