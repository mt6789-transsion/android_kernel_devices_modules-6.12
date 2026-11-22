// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * OCP81375 LED flashlight driver.
 */

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "flashlight-core.h"

#define OCP81375_NAME            "flashlights-ocp81375"
#define OCP81375_CHANNEL_NUM     2
#define OCP81375_CHANNEL_CH1     0
#define OCP81375_CHANNEL_CH2     1

#define OCP81375_REG_ENABLE      0x01
#define OCP81375_REG_FLASH_LED1  0x03
#define OCP81375_REG_FLASH_LED2  0x04
#define OCP81375_REG_TORCH_LED1  0x05
#define OCP81375_REG_TORCH_LED2  0x06
#define OCP81375_REG_BOOST_CONF  0x07
#define OCP81375_REG_TIMING      0x08
#define OCP81375_REG_FLAGS1      0x0a
#define OCP81375_REG_FLAGS2      0x0b
#define OCP81375_REG_DEVICE_ID   0x0c

#define OCP81375_DEVICE_ID       0x3a

#define OCP81375_MODE_OFF        0
#define OCP81375_MODE_TORCH      2
#define OCP81375_MODE_FLASH      3

struct ocp81375_platform_data {
	int channel_num;
	int _reserved[3];
};

struct ocp81375_chip_data {
	struct i2c_client *client;
	struct ocp81375_platform_data *pdata;
	struct mutex lock;
};

struct ocp81375_channel_ctx {
	u8 duty;
	u8 mode;
	u16 _pad;
	u32 current_ma;
	struct hrtimer timer;
	u32 timeout_ms;
	u32 _pad2;
	struct work_struct work;
	u8 duty_num;
	u8 _pad3[7];
	const u32 *current_table;
};

struct ocp81375_dev {
	int use_count;
	struct mutex lock;
	struct regulator *regulator;
	struct i2c_client *client;
	struct ocp81375_channel_ctx ch[OCP81375_CHANNEL_NUM];
};

struct ocp81375_dev ocp81375_device;

static const u32 back_channel_current[30] = {
	  32,   97,  162,  227,  292,  357,  422,  487,  552,  617,
	 682,  747,  812,  877,  942, 1007, 1072, 1137, 1202, 1267,
	1332, 1397, 1462, 1527, 1592, 1657, 1722, 1787, 1852, 1917,
};

static const u32 front_channel_current[2] = {
	200, 550,
};

static int ocp81375_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	struct ocp81375_chip_data *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);
	if (ret < 0)
		pr_err("failed writing at 0x%02x\n", reg);

	return ret;
}

static int ocp81375_read_reg(struct i2c_client *client, u8 reg)
{
	struct ocp81375_chip_data *chip = i2c_get_clientdata(client);
	int val;

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);
	if (val < 0)
		pr_err("failed reading at 0x%02x\n", reg);

	return val;
}

static void ocp81375_get_hw_fault(void)
{
	ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_FLAGS1);
	ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_FLAGS2);
}

static void ocp81375_set_current(int channel, int current_ma)
{
	u8 code;

	if (channel == OCP81375_CHANNEL_CH1) {
		if (current_ma >= 501) {
			ocp81375_device.ch[0].mode = OCP81375_MODE_FLASH;
			code = ((current_ma * 100 - 1563) / 1563) & 0x7f;
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_FLASH_LED2, code);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TORCH_LED2, 0);
		} else {
			ocp81375_device.ch[0].mode = OCP81375_MODE_TORCH;
			code = (current_ma < 10) ? 0 : ((current_ma * 100 - 390) / 390);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TORCH_LED2, code);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_FLASH_LED2, 0);
		}
	} else if (channel == OCP81375_CHANNEL_CH2) {
		if (current_ma >= 501) {
			ocp81375_device.ch[1].mode = OCP81375_MODE_FLASH;
			code = ((current_ma * 100 - 1563) / 1563) & 0x7f;
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_FLASH_LED1, code);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TORCH_LED1, 0);
		} else {
			ocp81375_device.ch[1].mode = OCP81375_MODE_TORCH;
			code = (current_ma < 10) ? 0 : ((current_ma * 100 - 390) / 390);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TORCH_LED1, code);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_FLASH_LED1, 0);
		}
	} else {
		pr_info("Error channel");
	}

	mdelay(3);
}

static void ocp81375_enable(int channel)
{
	int val;
	ktime_t ktime;

	if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
		BUG();

	if (ocp81375_device.ch[channel].timeout_ms &&
	    ocp81375_device.ch[channel].mode == OCP81375_MODE_FLASH) {
		unsigned int s = ocp81375_device.ch[channel].timeout_ms / 1000;
		unsigned int ms = ocp81375_device.ch[channel].timeout_ms % 1000;

		ktime = ktime_set(s, ms * 1000000);
		hrtimer_start(&ocp81375_device.ch[channel].timer, ktime, HRTIMER_MODE_REL);
	}

	if (channel == OCP81375_CHANNEL_CH1) {
		if (ocp81375_device.ch[0].current_ma >= 501) {
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, 0x0e);
		} else {
			val = ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_ENABLE);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE,
					   (val & 0x01) | 0x0a);
		}
	} else if (channel == OCP81375_CHANNEL_CH2) {
		if (ocp81375_device.ch[1].current_ma >= 501) {
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, 0x0d);
		} else {
			val = ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_ENABLE);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE,
					   (val & 0x02) | 0x09);
		}
	} else {
		pr_info("Error channel");
	}
}

static void ocp81375_disable(int channel)
{
	int val;

	val = ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_ENABLE);
	if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
		BUG();

	if (ocp81375_device.ch[channel].mode == OCP81375_MODE_FLASH)
		hrtimer_cancel(&ocp81375_device.ch[channel].timer);

	if (channel == OCP81375_CHANNEL_CH1) {
		ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, val & 0x0d);
	} else if (channel == OCP81375_CHANNEL_CH2) {
		ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, val & 0x0e);
	} else {
		pr_info("Error channel");
	}

	ocp81375_device.ch[channel].mode = OCP81375_MODE_OFF;
}

static void ocp81375_work_disable_ch1(struct work_struct *data)
{
	if (debug_mask)
		pr_info("ht work queue callback\n");
	ocp81375_disable(OCP81375_CHANNEL_CH1);
}

static void ocp81375_work_disable_ch2(struct work_struct *data)
{
	if (debug_mask)
		pr_info("lt work queue callback\n");
	ocp81375_disable(OCP81375_CHANNEL_CH2);
}

static enum hrtimer_restart ocp81375_timer_func_ch1(struct hrtimer *timer)
{
	schedule_work(&ocp81375_device.ch[0].work);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ocp81375_timer_func_ch2(struct hrtimer *timer)
{
	schedule_work(&ocp81375_device.ch[1].work);
	return HRTIMER_NORESTART;
}

static void ocp81375_uninit(void)
{
	if (debug_mask)
		pr_info("%s:%d start", __func__, __LINE__);

	ocp81375_device.ch[0].duty = 0;
	ocp81375_device.ch[0].mode = 4;
	ocp81375_device.ch[1].duty = 0;
	ocp81375_device.ch[1].mode = 4;
	ocp81375_device.ch[0].current_ma = 0;
	ocp81375_device.ch[1].current_ma = 0;

	ocp81375_get_hw_fault();
	if (ocp81375_device.regulator)
		regulator_disable(ocp81375_device.regulator);

	if (debug_mask)
		pr_info("%s:%d end", __func__, __LINE__);
}

static int ocp81375_set_driver(int set)
{
	mutex_lock(&ocp81375_device.lock);
	if (set) {
		if (!ocp81375_device.use_count) {
			ocp81375_device.ch[0].duty = 0;
			ocp81375_device.ch[0].mode = 0;
			ocp81375_device.ch[1].duty = 0;
			ocp81375_device.ch[1].mode = 0;
			ocp81375_device.ch[0].current_ma = 0;
			ocp81375_device.ch[1].current_ma = 0;

			if (ocp81375_device.regulator)
				regulator_enable(ocp81375_device.regulator);
			mdelay(3);

			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, 0x00);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_BOOST_CONF, 0x09);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TIMING, 0x1f);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_FLASH_LED1, 0x00);
			ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_TORCH_LED1, 0x00);
		}
		ocp81375_device.use_count++;
		if (debug_mask)
			pr_info("Set driver: %d\n", ocp81375_device.use_count);
	} else {
		ocp81375_device.use_count--;
		if (ocp81375_device.use_count == 0)
			ocp81375_uninit();
		if (ocp81375_device.use_count < 0)
			ocp81375_device.use_count = 0;
		if (debug_mask)
			pr_info("Unset driver: %d\n", ocp81375_device.use_count);
	}
	mutex_unlock(&ocp81375_device.lock);
	return 0;
}

static int ocp81375_open(void)
{
	return 0;
}

static int ocp81375_release(void)
{
	if (debug_mask)
		pr_info("Release: %d\n", ocp81375_device.use_count);
	return 0;
}

static int ocp81375_ioctl(unsigned int cmd, unsigned long arg)
{
	struct flashlight_dev_arg *fl_arg = (struct flashlight_dev_arg *)arg;
	int channel = fl_arg->channel;

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_TIME_OUT_TIME_MS(%d): %d\n", channel, fl_arg->arg);
		if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
			BUG();
		ocp81375_device.ch[channel].timeout_ms = fl_arg->arg;
		break;

	case FLASH_IOC_SET_DUTY:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_DUTY(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg < 0)
			fl_arg->arg = 0;
		if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
			BUG();
		ocp81375_device.ch[channel].current_ma =
			ocp81375_device.ch[channel].current_table[fl_arg->arg];
		ocp81375_device.ch[channel].duty = fl_arg->arg;
		ocp81375_set_current(channel, ocp81375_device.ch[channel].current_ma);
		break;

	case FLASH_IOC_SET_CURRENT:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_CURRENT(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg >= 2000)
			fl_arg->arg = 1999;
		if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
			BUG();
		ocp81375_device.ch[channel].current_ma = fl_arg->arg;
		ocp81375_device.ch[channel].duty = 0;
		ocp81375_set_current(channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n", channel, fl_arg->arg);
		if (fl_arg->arg == 1)
			ocp81375_enable(channel);
		else
			ocp81375_disable(channel);
		break;

	case FLASH_IOC_PRE_ON:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_ONOFF(%d): %d\n", channel, fl_arg->arg);
		break;

	case FLASH_IOC_SET_SCENARIO:
		if (debug_mask)
			pr_info("FLASH_IOC_SET_SCENARIO(%d): %d\n", channel, fl_arg->arg);
		break;

	case FLASH_IOC_GET_DUTY_NUMBER:
		if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
			BUG();
		if (debug_mask)
			pr_info("FLASH_IOC_GET_DUTY_NUMBER(%d)\n",
				ocp81375_device.ch[channel].duty_num);
		fl_arg->arg = ocp81375_device.ch[channel].duty_num;
		break;

	case FLASH_IOC_GET_MAX_TORCH_DUTY:
		if (debug_mask)
			pr_info("FLASH_IOC_GET_MAX_TORCH_DUTY\n");
		fl_arg->arg = (channel == 0) ? 8 : 1;
		break;

	case FLASH_IOC_GET_DUTY_CURRENT:
		if (debug_mask)
			pr_info("FLASH_IOC_GET_DUTY_CURRENT(%d): %d\n", channel, fl_arg->arg);
		if ((unsigned int)channel >= OCP81375_CHANNEL_NUM)
			BUG();
		if (fl_arg->arg >= ocp81375_device.ch[channel].duty_num)
			fl_arg->arg = ocp81375_device.ch[channel].duty_num - 1;
		fl_arg->arg = ocp81375_device.ch[channel].current_table[fl_arg->arg];
		break;

	case FLASH_IOC_GET_HW_FAULT:
		if (debug_mask)
			pr_info("FLASH_IOC_GET_HW_FAULT(%d)\n", channel);
		ocp81375_get_hw_fault();
		fl_arg->arg = 0;
		break;

	case FLASH_IOC_GENERATE_DEV_PARA:
		if (debug_mask)
			pr_info("FLASH_IOC_GENERATE_DEV_PARA(%d): %d\n", channel, fl_arg->arg);
		break;

	default:
		pr_info("No such command and arg(%d): (%d, %d)\n",
			channel, _IOC_NR(cmd), fl_arg->arg);
		return -ENOTTY;
	}

	return 0;
}

static ssize_t ocp81375_strobe_store(struct flashlight_arg fl_arg)
{
	ocp81375_set_driver(0);
	if ((unsigned int)fl_arg.channel >= OCP81375_CHANNEL_NUM)
		BUG();

	ocp81375_device.ch[fl_arg.channel].current_ma =
		ocp81375_device.ch[fl_arg.channel].current_table[fl_arg.level];
	ocp81375_set_current(fl_arg.channel, ocp81375_device.ch[fl_arg.channel].current_ma);
	ocp81375_enable(fl_arg.channel);
	if (fl_arg.dur > 0) {
		int i;

		for (i = 0; i < fl_arg.dur; i++)
			mdelay(1);
	}
	ocp81375_disable(fl_arg.channel);
	if (debug_mask)
		pr_info("Release: %d\n", ocp81375_device.use_count);

	return 0;
}

static struct flashlight_operations ocp81375_ops = {
	.flashlight_open = ocp81375_open,
	.flashlight_release = ocp81375_release,
	.flashlight_ioctl = ocp81375_ioctl,
	.flashlight_strobe_store = ocp81375_strobe_store,
	.flashlight_set_driver = ocp81375_set_driver,
};

static int ocp81375_i2c_probe(struct i2c_client *client)
{
	struct ocp81375_chip_data *chip;
	struct ocp81375_platform_data *pdata = dev_get_platdata(&client->dev);
	int dev_id;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("Failed to check i2c functionality.\n");
		return -ENODEV;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	if (!pdata) {
		pr_err("Platform data does not exist\n");
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	}
	chip->pdata = pdata;

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);
	ocp81375_device.client = client;
	mutex_init(&ocp81375_device.lock);

	INIT_WORK(&ocp81375_device.ch[0].work, ocp81375_work_disable_ch1);
	INIT_WORK(&ocp81375_device.ch[1].work, ocp81375_work_disable_ch2);

	hrtimer_init(&ocp81375_device.ch[0].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81375_device.ch[0].timer.function = ocp81375_timer_func_ch1;
	hrtimer_init(&ocp81375_device.ch[1].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ocp81375_device.ch[1].timer.function = ocp81375_timer_func_ch2;

	ocp81375_device.ch[0].timeout_ms = 100;
	ocp81375_device.ch[1].timeout_ms = 100;

	if (ocp81375_device.regulator)
		regulator_enable(ocp81375_device.regulator);
	mdelay(3);

	dev_id = ocp81375_read_reg(ocp81375_device.client, OCP81375_REG_DEVICE_ID);
	ocp81375_write_reg(ocp81375_device.client, OCP81375_REG_ENABLE, 0x00);

	if (ocp81375_device.regulator)
		regulator_disable(ocp81375_device.regulator);

	dev_id &= 0xffff;
	pr_info("ocp81375 read DEVICE_ID = 0x%x", dev_id);
	if (dev_id != OCP81375_DEVICE_ID) {
		pr_err("ocp81375 DEVICE_ID mismatch, skip.");
		goto err_free;
	}

	if (flashlight_dev_register(OCP81375_NAME, &ocp81375_ops)) {
		pr_err("ocp81375 flashlight_dev_register fail.");
		goto err_free;
	}

	ocp81375_device.ch[0].current_table = back_channel_current;
	ocp81375_device.ch[0].duty_num = ARRAY_SIZE(back_channel_current);
	ocp81375_device.ch[1].current_table = front_channel_current;
	ocp81375_device.ch[1].duty_num = ARRAY_SIZE(front_channel_current);
	ocp81375_device.use_count = 0;

	return 0;

err_free:
	kfree(chip->pdata);
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	return -EFAULT;
}

static void ocp81375_i2c_remove(struct i2c_client *client)
{
	flush_work(&ocp81375_device.ch[0].work);
	flush_work(&ocp81375_device.ch[1].work);
	flashlight_dev_unregister(OCP81375_NAME);
}

static const struct i2c_device_id ocp81375_i2c_id[] = {
	{ OCP81375_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ocp81375_i2c_id);

static const struct of_device_id ocp81375_i2c_of_match[] = {
	{ .compatible = "mediatek,strobe_main" },
	{ }
};
MODULE_DEVICE_TABLE(of, ocp81375_i2c_of_match);

static struct i2c_driver ocp81375_i2c_driver = {
	.driver = {
		.name = OCP81375_NAME,
		.of_match_table = ocp81375_i2c_of_match,
	},
	.probe = ocp81375_i2c_probe,
	.remove = ocp81375_i2c_remove,
	.id_table = ocp81375_i2c_id,
};

static int ocp81375_probe(struct platform_device *pdev)
{
	ocp81375_device.regulator = regulator_get(&pdev->dev, "ocp81375_regulator");
	if (IS_ERR(ocp81375_device.regulator)) {
		ocp81375_device.regulator = NULL;
		pr_err("ocp81375_device.regulator get fail!");
	}

	if (i2c_add_driver(&ocp81375_i2c_driver))
		return -1;

	return 0;
}

static void ocp81375_remove(struct platform_device *pdev)
{
	i2c_del_driver(&ocp81375_i2c_driver);
}

static const struct of_device_id ocp81375_of_match[] = {
	{ .compatible = "mediatek,flashlights_ocp81375" },
	{ }
};
MODULE_DEVICE_TABLE(of, ocp81375_of_match);

static struct platform_driver ocp81375_platform_driver = {
	.probe = ocp81375_probe,
	.remove = ocp81375_remove,
	.driver = {
		.name = OCP81375_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ocp81375_of_match,
	},
};

static int __init flashlight_ocp81375_init(void)
{
	int ret;

	ret = platform_driver_register(&ocp81375_platform_driver);
	if (ret)
		pr_err("Failed to register platform driver\n");

	return ret;
}

static void __exit flashlight_ocp81375_exit(void)
{
	platform_driver_unregister(&ocp81375_platform_driver);
}

module_init(flashlight_ocp81375_init);
module_exit(flashlight_ocp81375_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("qinlong.lu <qinlong.lu@transsion.com>");
MODULE_DESCRIPTION("TRANSSION Flashlight OCP81375 Driver");
