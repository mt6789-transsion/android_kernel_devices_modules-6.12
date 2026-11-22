// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * Southchip SC8950 Switching Charger Driver.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy/phy.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/workqueue.h>

#include "charger_class.h"

#define SC8950_REG_00		0x00
#define SC8950_REG_01		0x01
#define SC8950_REG_02		0x02
#define SC8950_REG_03		0x03
#define SC8950_REG_04		0x04
#define SC8950_REG_05		0x05
#define SC8950_REG_06		0x06
#define SC8950_REG_07		0x07
#define SC8950_REG_08		0x08
#define SC8950_REG_09		0x09
#define SC8950_REG_0A		0x0a
#define SC8950_REG_0B		0x0b
#define SC8950_REG_0C		0x0c
#define SC8950_REG_0D		0x0d
#define SC8950_REG_11		0x11
#define SC8950_REG_13		0x13
#define SC8950_REG_14		0x14
#define SC8950_REG_NUM		0x15

#define SC8950_REG_KEY		0x7d
#define SC8950_REG_HIDDEN	0x83

#define PHY_MODE_BC11_SET	1
#define PHY_MODE_BC11_CLR	2

struct sc8950_chip {
	struct i2c_client *client;
	struct device *dev;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct mutex sc8950_i2c_access;
	atomic_t attach;
	int part_no;
	const char *chg_name;
	int irq;
	int intr_gpio;
	struct work_struct irq_work;
	struct workqueue_struct *irq_wq;
	struct power_supply_desc psy_desc;
	struct power_supply *psy;
	enum power_supply_usb_type psy_usb_type;
	struct work_struct bc12_work;
	int boost_mode;
	atomic_t online;
	struct mutex attach_lock;
	struct regulator_dev *otg_rdev;
	bool vbus_ovp_flag;
};

static struct timespec64 ptime[13];

static const u32 sc8950_otg_oc_threshold[] = {
	500000, 750000, 1200000, 1400000,
	1650000, 1875000, 2150000, 2450000,
};

static const char * const dpdm_ctrl_name[] = {
	"DPDM_CTRL_HZ",
	"DPDM_CTRL_0V",
	"DPDM_CTRL_0_6V",
	"DPDM_CTRL_3_3V",
};

static char *sc8950_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger",
};

static const u32 sc8950_charger_usb_types =
	BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
	BIT(POWER_SUPPLY_USB_TYPE_SDP) |
	BIT(POWER_SUPPLY_USB_TYPE_DCP) |
	BIT(POWER_SUPPLY_USB_TYPE_CDP) |
	BIT(POWER_SUPPLY_USB_TYPE_C) |
	BIT(POWER_SUPPLY_USB_TYPE_PD) |
	BIT(POWER_SUPPLY_USB_TYPE_PD_DRP) |
	BIT(POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID);

static enum power_supply_property sc8950_charger_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int __sc8950_read_byte(struct sc8950_chip *info, u8 reg, u8 *data)
{
	int retry = 0;
	int ret;
	u8 reg_addr = reg;

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = info->client->addr,
				.flags = 0,
				.len = 1,
				.buf = &reg_addr,
			},
			{
				.addr = info->client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = data,
			},
		};

		ret = i2c_transfer(info->client->adapter, msgs, 2);
		if (ret == 2)
			break;
		if (ret == -ENXIO) {
			dev_err(info->dev, "skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}
		mdelay(10);
	} while (++retry < 3);

	return (ret == 2) ? 0 : -1;
}

static int sc8950_read_byte(struct sc8950_chip *info, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&info->sc8950_i2c_access);
	ret = __sc8950_read_byte(info, reg, data);
	mutex_unlock(&info->sc8950_i2c_access);
	return ret;
}

static int __sc8950_write_byte(struct sc8950_chip *info, u8 reg, u8 val)
{
	int retry = 0;
	int ret;
	u8 buf[2] = { reg, val };

	do {
		struct i2c_msg msg = {
			.addr = info->client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		};

		ret = i2c_transfer(info->client->adapter, &msg, 1);
		if (ret == 1)
			break;
		if (ret == -ENXIO) {
			dev_err(info->dev, "skipping non-existent adapter %s\n",
				info->client->adapter->name);
			break;
		}
		mdelay(10);
	} while (++retry < 3);

	return (ret == 1) ? 1 : -1;
}

static int sc8950_write_byte(struct sc8950_chip *info, u8 reg, u8 val)
{
	int ret;

	mutex_lock(&info->sc8950_i2c_access);
	ret = __sc8950_write_byte(info, reg, val);
	mutex_unlock(&info->sc8950_i2c_access);
	return ret;
}

static int sc8950_update_bits(struct sc8950_chip *info, u8 reg, u8 val, u8 mask, u8 shift)
{
	u8 reg_val = 0, tmp = 0;
	int ret;

	mutex_lock(&info->sc8950_i2c_access);
	__sc8950_read_byte(info, reg, &reg_val);
	reg_val &= ~(mask << shift);
	reg_val |= (val << shift);
	__sc8950_write_byte(info, reg, reg_val);
	ret = __sc8950_read_byte(info, reg, &tmp);
	mutex_unlock(&info->sc8950_i2c_access);
	return ret;
}

static int sc8950_read_interface(struct sc8950_chip *info, u8 reg, u8 *val, u8 mask, u8 shift)
{
	u8 reg_val = 0;
	int ret;

	mutex_lock(&info->sc8950_i2c_access);
	ret = __sc8950_read_byte(info, reg, &reg_val);
	reg_val &= (mask << shift);
	*val = reg_val >> shift;
	mutex_unlock(&info->sc8950_i2c_access);
	return ret;
}

static int sc8950_key_toggle(struct sc8950_chip *info)
{
	int ret;

	sc8950_write_byte(info, SC8950_REG_KEY, 0x48);
	sc8950_write_byte(info, SC8950_REG_KEY, 0x54);
	sc8950_write_byte(info, SC8950_REG_KEY, 0x53);
	ret = sc8950_write_byte(info, SC8950_REG_KEY, 0x38);
	return ret;
}

static int sc8950_set_hiz(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s hiz_en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_00, en, 0x1, 7);
}

static int sc8950_set_ibus(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s ibus:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_00, val, 0x3f, 0);
}

static int sc8950_set_dp(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s dp:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_01, val, 0x7, 5);
}

static int sc8950_set_dm(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s dm:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_01, val, 0x7, 2);
}

static int sc8950_set_ico(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s ico_en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_02, en, 0x1, 4);
}

static int sc8950_set_hvdcp(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s hvdcp_en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_02, en, 0x1, 3);
}

static int sc8950_set_force_dpdm(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s force_dpdm_en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_02, en, 0x1, 1);
}

static int sc8950_set_autodpdm(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s autodpdm_en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_02, en, 0x1, 0);
}

static int sc8950_set_wdt_rst(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s wdt_rst:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_03, val, 0x1, 6);
}

static int sc8950_set_en_otg(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s otg en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_03, en, 0x1, 5);
}

static int sc8950_set_en_charge(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s charge en:0x%x,boost_mode:%d\n",
		 __func__, en, info->boost_mode);
	if (info->boost_mode && en)
		return 0;
	return sc8950_update_bits(info, SC8950_REG_03, en, 0x1, 4);
}

static int sc8950_set_vsys_min(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s vsys_min:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_03, val, 0x7, 1);
}

static int sc8950_set_vbatlow_otg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s vbatlow_otg:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_03, val, 0x1, 0);
}

static int sc8950_set_icc(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s icc:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_04, val, 0x7f, 0);
}

static int sc8950_set_iprechg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s iprechg:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_05, val, 0xf, 4);
}

static int sc8950_set_iterm(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s iterm:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_05, val, 0xf, 0);
}

static int sc8950_set_vreg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s vreg:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_06, val, 0x3f, 2);
}

static int sc8950_set_vrechg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s vrechg:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_06, val, 0x1, 0);
}

static int sc8950_set_en_term(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s term en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_07, en, 0x1, 7);
}

static int sc8950_set_statdis(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s STAT pin function en:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_07, val, 0x1, 6);
}

static int sc8950_set_watchdog_cfg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s wtd_cfg:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_07, val, 0x3, 4);
}

static int sc8950_set_en_timer(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s safety timer en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_07, en, 0x1, 3);
}

static int sc8950_set_tchg(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s charge time:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_07, val, 0x3, 1);
}

static int sc8950_set_chg_thermal(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s chg_thermal:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_08, val, 0x3, 0);
}

static int sc8950_set_batfet_reset_en(struct sc8950_chip *info, u8 en)
{
	dev_info(info->dev, "%s batfet reset en:0x%x\n", __func__, en);
	return sc8950_update_bits(info, SC8950_REG_09, en, 0x1, 2);
}

static int sc8950_set_boostv(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s boostv:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_0A, val, 0xf, 4);
}

static int sc8950_set_iboost(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s iboost:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_0A, val, 0x7, 0);
}

static int sc8950_set_rst(struct sc8950_chip *info, u8 val)
{
	dev_info(info->dev, "%s rst:0x%x\n", __func__, val);
	return sc8950_update_bits(info, SC8950_REG_14, val, 0x1, 7);
}

static int sc8950_set_force_vindpm(struct sc8950_chip *info, u8 val)
{
	u8 force = 0;
	int retry = 0;

	dev_info(info->dev, "%s force vindpm:0x%x\n", __func__, val);
	sc8950_read_interface(info, SC8950_REG_0D, &force, 0x1, 7);
	while (!force && retry < 3) {
		sc8950_update_bits(info, SC8950_REG_0D, 1, 0x1, 7);
		msleep(5);
		sc8950_read_interface(info, SC8950_REG_0D, &force, 0x1, 7);
		retry++;
	}
	return sc8950_update_bits(info, SC8950_REG_0D, val, 0x7f, 0);
}

static int __sc8950_dump_registers(struct sc8950_chip *info)
{
	u8 reg, val = 0, hidden = 0;
	int ret;

	for (reg = 0; reg < SC8950_REG_NUM; reg++) {
		ret = sc8950_read_byte(info, reg, &val);
		if (ret < 0)
			return ret;
		dev_info(info->dev, "%s reg[0x%02x] = 0x%02x\n", __func__, reg, val);
	}

	ret = sc8950_read_byte(info, SC8950_REG_HIDDEN, &hidden);
	if (ret < 0)
		sc8950_key_toggle(info);
	sc8950_read_byte(info, SC8950_REG_HIDDEN, &hidden);
	sc8950_key_toggle(info);
	dev_info(info->dev, "%s reg[0x%02x] = 0x%02x\n",
		 __func__, SC8950_REG_HIDDEN, hidden);
	return 0;
}

static int sc8950_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_0B, &val, 0x3, 3);
	if (ret < 0) {
		dev_err(info->dev, "%s get chg stat fail(%d)\n", __func__, ret);
		return ret;
	}
	*done = (val == 3);
	dev_info(info->dev, "%s done:%d\n", __func__, *done);
	return ret;
}

static int sc8950_vg_irq_handler(struct sc8950_chip *info)
{
	u8 val = 0;

	sc8950_read_interface(info, SC8950_REG_11, &val, 0x1, 7);
	pr_info("%s vg_stat:%d\n", __func__, val);
	return 0;
}

static int sc8950_eoc_irq_handler(struct sc8950_chip *info)
{
	bool done = false;

	sc8950_is_charging_done(info->chg_dev, &done);
	if (done) {
		dev_err(info->dev, "%s happen!\n", __func__);
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_EOC);
	}
	return 0;
}

static int sc8950_fault_irq_handler(struct sc8950_chip *info)
{
	struct chgdev_notify *noti = charger_dev_get_notify(info->chg_dev);
	u8 fault = 0;
	int ret;

	ret = sc8950_read_byte(info, SC8950_REG_0C, &fault);
	if (ret < 0)
		dev_err(info->dev, "%s read reg[0x%x] failed!\n",
			"sc8950_check_fault_state", SC8950_REG_0C);

	if (fault & BIT(7))
		dev_err(info->dev, "%s Watchdog timer expiration!reg[c]=0x%x\n",
			__func__, fault);

	if (fault & BIT(6)) {
		dev_err(info->dev, "%s vbus ovp!reg[c]=0x%x\n", __func__, fault);
		info->vbus_ovp_flag = true;
		noti->vbusov_stat = true;
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	} else if (info->vbus_ovp_flag) {
		info->vbus_ovp_flag = false;
		noti->vbusov_stat = false;
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
	}

	switch ((fault >> 4) & 0x3) {
	case 0:
		if (info->vbus_ovp_flag) {
			info->vbus_ovp_flag = false;
			noti->vbusov_stat = false;
			charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
		}
		break;
	case 1:
		dev_err(info->dev, "%s vac ovp!reg[c]=0x%x\n", __func__, fault);
		info->vbus_ovp_flag = true;
		noti->vbusov_stat = true;
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
		break;
	case 2:
		dev_err(info->dev, "%s thermal shutdown!reg[c]=0x%x\n", __func__, fault);
		break;
	case 3:
		dev_err(info->dev, "%s safety timer expiration!reg[c]=0x%x\n",
			__func__, fault);
		break;
	}

	if (fault & BIT(3))
		dev_err(info->dev, "%s bat ovp!reg[c]=0x%x\n", __func__, fault);

	switch (fault & 0x7) {
	case 2:
		dev_err(info->dev, "%s ntc warm!reg[c]=0x%x\n", __func__, fault);
		break;
	case 3:
		dev_err(info->dev, "%s ntc cool !reg[c]=0x%x\n", __func__, fault);
		break;
	case 5:
		dev_err(info->dev, "%s ntc cold !reg[c]=0x%x\n", __func__, fault);
		break;
	case 6:
		dev_err(info->dev, "%s ntc hot !reg[c]=0x%x\n", __func__, fault);
		break;
	default:
		break;
	}

	return 0;
}

static void sc8950_work_handler(struct work_struct *work)
{
	struct sc8950_chip *info = container_of(work, struct sc8950_chip, irq_work);

	disable_irq(info->irq);
	dev_info(info->dev, "%s\n", __func__);
	sc8950_vg_irq_handler(info);
	sc8950_eoc_irq_handler(info);
	sc8950_fault_irq_handler(info);
	enable_irq(info->irq);
}

static irqreturn_t sc8950_irq_handler(int irq, void *data)
{
	struct sc8950_chip *info = data;

	dev_info(info->dev, "%s\n", __func__);
	queue_work(info->irq_wq, &info->irq_work);
	return IRQ_NONE;
}

static void sc8950_chg_set_usbsw(struct sc8950_chip *info, int usbsw)
{
	struct phy *phy;
	int ret;

	dev_info(info->dev, "%s:usbsw=%d\n", __func__, usbsw);
	phy = phy_get(info->dev, "usb2-phy");
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(info->dev, "%s:failed to get usb2-phy\n", __func__);
		return;
	}

	ret = phy_set_mode_ext(phy, PHY_MODE_USB_DEVICE,
			       usbsw ? PHY_MODE_BC11_CLR : PHY_MODE_BC11_SET);
	if (ret)
		dev_err(info->dev, "%s:failed to set phy ext mode\n", __func__);

	phy_put(info->dev, phy);
}

static void sc8950_chg_enable_bc12(struct sc8950_chip *info, bool en)
{
	int retry = 250;

	dev_info(info->dev, "%s:en=%d\n", __func__, en);
	if (en) {
		dev_info(info->dev, "check CDP block\n");
		while (retry > 0) {
			struct device_node *usb_np;
			struct property *prop;

			usb_np = of_parse_phandle(info->dev->of_node, "usb", 0);
			if (!usb_np) {
				dev_notice(info->dev, "usb node missing or invalid\n");
				dev_info(info->dev, "CDP free\n");
				break;
			}
			prop = of_find_property(usb_np, "cdp-block", NULL);
			dev_info(info->dev, "usb ready = %d\n", !prop);
			if (!prop) {
				dev_info(info->dev, "CDP free\n");
				break;
			}
			if (!atomic_read(&info->attach) || !atomic_read(&info->online)) {
				dev_notice(info->dev, "Plug out or shutdown!\n");
				return;
			}
			msleep(100);
			retry--;
		}
		if (retry == 0)
			dev_notice(info->dev, "CDP timeout\n");
	}

	sc8950_chg_set_usbsw(info, !en);
}

static int sc8950_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	dev_info(info->dev, "%s powerpath en:0x%x\n", __func__, en);
	return sc8950_set_hiz(info, !en);
}

static void sc8950_bc12_detect(struct work_struct *work)
{
	struct sc8950_chip *info = container_of(work, struct sc8950_chip, bc12_work);
	bool attach;
	u8 val = 0, pg = 0, vbus_stat = 0;
	int count, ret;

	mutex_lock(&info->attach_lock);
	attach = atomic_read(&info->attach) != 0;

	if (atomic_read(&info->online) == attach &&
	    !(info->psy_desc.type == POWER_SUPPLY_TYPE_USB &&
	      (info->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN ||
	       info->psy_usb_type == POWER_SUPPLY_USB_TYPE_DCP))) {
		dev_info(info->dev, "%s attach(%d) is the same\n", __func__, attach);
		goto out_unlock;
	}

	atomic_set(&info->online, attach);
	pr_info("%s: attach= 0x%X\n", __func__, attach);

	if (!attach || info->boost_mode) {
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		goto out_unlock;
	}

	sc8950_chg_enable_bc12(info, true);
	sc8950_enable_powerpath(info->chg_dev, true);

	for (count = 0; count < 10; count++) {
		if (!atomic_read(&info->attach))
			break;
		msleep(20);
		sc8950_read_interface(info, SC8950_REG_11, &val, 0x1, 7);
		pr_info("%s vg_stat:%d\n", __func__, val);
		if (val)
			break;
	}

	msleep(100);
	sc8950_set_force_dpdm(info, 1);

	for (count = 0; count < 16; count++) {
		if (!atomic_read(&info->attach))
			break;
		msleep(30);
		ret = sc8950_read_byte(info, SC8950_REG_0B, &val);
		if (ret < 0) {
			dev_err(info->dev, "%s failed!\n", "sc8950_check_bc12_done");
			pg = 0;
		} else {
			pg = (val >> 2) & 0x1;
		}
		sc8950_read_interface(info, SC8950_REG_11, &val, 0x1, 7);
		pr_info("%s: pg_state=%d  count=%d\n", __func__, pg, count);
		if (!val) {
			pr_info("%s vbus(%d) not good, exit that get charger type!\n",
				__func__, 0);
			break;
		}
		if (pg)
			break;
	}

	sc8950_read_interface(info, SC8950_REG_0B, &vbus_stat, 0x7, 5);
	pr_info("%s: usb_stats= 0x%X\n", __func__, vbus_stat);

	switch (vbus_stat) {
	case 1:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case 2:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case 3:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	default:
		info->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		info->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	}

out_unlock:
	mutex_unlock(&info->attach_lock);
	pr_info("%s: type=%d usb_type=%d\n",
		__func__, info->psy_desc.type, info->psy_usb_type);

	if (info->psy_desc.type != POWER_SUPPLY_TYPE_USB_DCP)
		sc8950_chg_enable_bc12(info, false);

	if (info->psy && !info->boost_mode)
		power_supply_changed(info->psy);
}

static int sc8950_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 force = 0, val = 0;
	int ret = 0;

	sc8950_read_interface(info, SC8950_REG_0D, &force, 0x1, 7);
	if (force) {
		u32 calc;

		dev_info(info->dev, "%s its force vindpm!\n", "sc8950_get_force_vindpm");
		ret = sc8950_read_interface(info, SC8950_REG_0D, &val, 0x7f, 0);
		calc = val * 100000;
		if (calc > 1600000)
			*uV = calc + 2600000;
		else
			*uV = 4200000;
	} else {
		dev_info(info->dev, "%s its not force vindpm,return!\n",
			 "sc8950_get_force_vindpm");
		*uV = 4200000;
	}
	dev_info(info->dev, "%s get mivr: %d uV\n", __func__, *uV);
	return ret;
}

static int sc8950_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 reg_val;

	if (uV > 15300000)
		uV = 15300000;
	dev_info(info->dev, "%s set %d uV\n", __func__, uV);
	reg_val = (uV - 2600000) / 100000;
	return sc8950_set_force_vindpm(info, reg_val);
}

static int sc8950_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_13, &val, 0x1, 7);
	if (ret < 0) {
		dev_err(info->dev, "%s get mivr_state failed(%d)\n", __func__, ret);
		return ret;
	}
	*in_loop = val != 0;
	dev_info(info->dev, "%s mivr_state:0x%x\n", __func__, *in_loop);
	return ret;
}

static int sc8950_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 reg_val;

	if (uV > 4848000)
		uV = 4848000;
	dev_info(info->dev, "%s set %d uV\n", __func__, uV);
	reg_val = (uV - 3840000) / 16000;
	return sc8950_set_vreg(info, reg_val);
}

static int sc8950_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_06, &val, 0x3f, 2);
	if (ret < 0) {
		dev_err(info->dev, "%s get cv fail(%d)\n", __func__, ret);
		return ret;
	}
	*uV = val * 16000 + 3840000;
	dev_info(info->dev, "%s uV:%d\n", __func__, *uV);
	return ret;
}

static int sc8950_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u32 clamped = uA;
	u8 reg_val;

	if (clamped < 100000)
		clamped = 100000;
	if (clamped > 3250000)
		clamped = 3250000;
	reg_val = (clamped - 100000) / 50000;
	dev_info(info->dev, "%s uA:%d reg_val:%d\n", __func__, uA, reg_val);
	return sc8950_set_ibus(info, reg_val);
}

static int sc8950_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_00, &val, 0x3f, 0);
	if (ret < 0) {
		dev_err(info->dev, "%s get ibus fail(%d)\n", __func__, ret);
		return ret;
	}
	*uA = val * 50000 + 100000;
	dev_info(info->dev, "%s uA:%d\n", __func__, *uA);
	return ret;
}

static int sc8950_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int sc8950_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 reg_val;

	if (uA > 5040000)
		uA = 5040000;
	dev_info(info->dev, "%s set %d uA\n", __func__, uA);
	reg_val = uA / 60000;
	return sc8950_set_icc(info, reg_val);
}

static int sc8950_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_04, &val, 0x7f, 0);
	if (ret < 0) {
		dev_err(info->dev, "%s get icc fail(%d)\n", __func__, ret);
		return ret;
	}
	*uA = val * 60000;
	dev_info(info->dev, "%s uA:%d\n", __func__, *uA);
	return ret;
}

static int sc8950_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	*uA = 0;
	dev_info(info->dev, "%s %d uA\n", __func__, *uA);
	return 0;
}

static int sc8950_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 reg_val;

	if (uA > 930000)
		uA = 930000;
	reg_val = (uA - 30000) / 60000;
	dev_info(info->dev, "%s set %d uA reg:0x%x\n", __func__, uA, reg_val);
	return sc8950_set_iterm(info, reg_val);
}

static int sc8950_get_ieoc(struct charger_device *chg_dev, u32 *uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_05, &val, 0xf, 0);
	if (ret < 0) {
		dev_err(info->dev, "%s get ieoc fail(%d)\n", __func__, ret);
		return ret;
	}
	*uA = val * 60000 + 30000;
	dev_info(info->dev, "%s uA:%d\n", __func__, *uA);
	return ret;
}

static int sc8950_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_03, &val, 0x1, 4);
	if (ret < 0) {
		dev_err(info->dev, "%s get charge stat fail(%d)\n", __func__, ret);
		return ret;
	}
	*en = val != 0;
	dev_info(info->dev, "%s en:%d\n", __func__, *en);
	return ret;
}

static int sc8950_enable_charging(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	int ret;

	dev_info(info->dev, "%s en:%d\n", __func__, en);
	ret = sc8950_set_en_charge(info, en);
	if (ret < 0)
		dev_err(info->dev, "%s enable charge fail(%d)\n", __func__, -1);
	return ret;
}

static int sc8950_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_00, &val, 0x1, 7);
	if (ret < 0) {
		dev_err(info->dev, "%s get powerpath_enabled state failed(%d)\n",
			__func__, ret);
		return ret;
	}
	*en = !val;
	dev_info(info->dev, "%s powerpath_enabled:0x%x\n", __func__, *en);
	return 0;
}

static int sc8950_en_hz(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	dev_info(info->dev, "%s powerpath en:0x%x\n", __func__, en);
	return sc8950_set_hiz(info, en);
}

static int sc8950_tc30_gpio_enable(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	dev_info(info->dev, "%s en:%d\n", __func__, en);
	ret = sc8950_read_byte(info, SC8950_REG_HIDDEN, &val);
	if (ret < 0)
		sc8950_key_toggle(info);
	sc8950_update_bits(info, SC8950_REG_HIDDEN, en ? 1 : 0, 0x1, 5);
	ret = sc8950_key_toggle(info);
	if (!en) {
		sc8950_set_dp(info, 0);
		sc8950_set_dm(info, 0);
	}
	return ret;
}

static int sc8950_plug_in(struct charger_device *chg_dev)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	int ret;

	dev_info(info->dev, "%s\n", __func__);
	ret = sc8950_set_en_charge(info, 1);
	if (ret < 0) {
		dev_err(info->dev, "%s dis charge fail(%d)\n", __func__, -1);
		return -1;
	}
	ret = sc8950_set_watchdog_cfg(info, 0);
	if (ret < 0)
		dev_err(info->dev, "%s set wdt failed ret:%d\n", __func__, ret);
	return ret;
}

static int sc8950_plug_out(struct charger_device *chg_dev)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	int ret;

	dev_info(info->dev, "%s\n", __func__);
	sc8950_tc30_gpio_enable(chg_dev, false);
	ret = sc8950_set_en_charge(info, 0);
	if (ret < 0) {
		dev_err(info->dev, "%s dis charge fail(%d)\n", __func__, -1);
		return -1;
	}
	ret = sc8950_set_watchdog_cfg(info, 0);
	if (ret < 0)
		dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
	info->vbus_ovp_flag = false;
	return ret;
}

static int sc8950_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	return sc8950_set_wdt_rst(info, 1);
}

static int sc8950_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	dev_info(info->dev, "%s event = %d\n", __func__, event);
	power_supply_changed(info->psy);
	return 0;
}

static int sc8950_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_07, &val, 0x1, 3);
	if (ret < 0) {
		dev_err(info->dev, "%s get safety_timer_enabled state failed(%d)\n",
			__func__, ret);
		return ret;
	}
	*en = val != 0;
	dev_info(info->dev, "%s safety_timer_enabled:0x%x\n", __func__, *en);
	return ret;
}

static int sc8950_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	return sc8950_set_en_timer(info, en);
}

static int sc8950_enable_te(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	return sc8950_set_en_term(info, en);
}

static int sc8950_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	int ret;

	info->boost_mode = en ? 1 : 0;
	if (en) {
		ret = sc8950_set_watchdog_cfg(info, 0);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, ret);
			return ret;
		}
		ret = sc8950_set_hiz(info, 0);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, -1);
			return -1;
		}
	}

	ret = sc8950_set_en_charge(info, !en);
	if (ret < 0) {
		dev_err(info->dev, "%s set charge fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = sc8950_set_en_otg(info, en);
	if (ret < 0) {
		dev_err(info->dev, "%s set otg fail(%d)\n", __func__, ret);
		return ret;
	}

	if (!en) {
		ret = sc8950_set_watchdog_cfg(info, 0);
		if (ret < 0) {
			dev_err(info->dev, "%s set wdt fail(%d)\n", __func__, -1);
			return -1;
		}
	}
	return 0;
}

static int sc8950_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u8 reg_val;

	if (uA <= 500000)
		reg_val = 0;
	else if (uA <= 750000)
		reg_val = 1;
	else if (uA <= 1200000)
		reg_val = 2;
	else if (uA <= 1400000)
		reg_val = 3;
	else if (uA <= 1650000)
		reg_val = 4;
	else if (uA <= 1875000)
		reg_val = 5;
	else if (uA <= 2150000)
		reg_val = 6;
	else
		reg_val = 7;

	dev_info(info->dev, "%s uA:%d reg_val:0x%x\n", __func__, uA, reg_val);
	return sc8950_set_iboost(info, reg_val);
}

static int sc8950_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	if (!info)
		return -EINVAL;
	atomic_set(&info->attach, en ? 1 : 0);
	mutex_lock(&info->attach_lock);
	schedule_work(&info->bc12_work);
	mutex_unlock(&info->attach_lock);
	return 0;
}

static int sc8950_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	return 0;
}

static int sc8950_dump_registers(struct charger_device *chg_dev)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	return __sc8950_dump_registers(info);
}

static int sc8950_set_pep20_current_pattern(struct charger_device *chg_dev, u32 uV)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	u32 code;
	int i, idx = 1;
	struct timespec64 diff;
	s64 diff_ms;

	dev_info(info->dev, "%s!\n", __func__);
	sc8950_set_ichg(chg_dev, 2000000);
	mdelay(20);
	sc8950_set_aicr(chg_dev, 0);
	mdelay(150);

	code = (uV - 5500000) / 500000;
	ktime_get_boottime_ts64(&ptime[0]);

	for (i = 4; i >= 0; i--) {
		sc8950_set_aicr(chg_dev, 800000);
		if (code & BIT(i)) {
			mdelay(90);
			ktime_get_boottime_ts64(&ptime[idx]);
			diff = timespec64_sub(ptime[idx], ptime[idx - 1]);
			diff_ms = diff.tv_nsec / 1000000;
			if (diff_ms < 90 || diff_ms > 115)
				return -1;
			sc8950_set_aicr(chg_dev, 0);
			mdelay(40);
			ktime_get_boottime_ts64(&ptime[idx + 1]);
			diff = timespec64_sub(ptime[idx + 1], ptime[idx]);
			diff_ms = diff.tv_nsec / 1000000;
			if (diff_ms < 30 || diff_ms > 65)
				return -1;
		} else {
			mdelay(40);
			ktime_get_boottime_ts64(&ptime[idx]);
			diff = timespec64_sub(ptime[idx], ptime[idx - 1]);
			diff_ms = diff.tv_nsec / 1000000;
			if (diff_ms < 30 || diff_ms > 65)
				return -1;
			sc8950_set_aicr(chg_dev, 0);
			mdelay(90);
			ktime_get_boottime_ts64(&ptime[idx + 1]);
			diff = timespec64_sub(ptime[idx + 1], ptime[idx]);
			diff_ms = diff.tv_nsec / 1000000;
			if (diff_ms < 90 || diff_ms > 115)
				return -1;
		}
		idx += 2;
	}

	sc8950_set_aicr(chg_dev, 800000);
	mdelay(200);
	ktime_get_boottime_ts64(&ptime[11]);
	diff = timespec64_sub(ptime[11], ptime[10]);
	diff_ms = diff.tv_nsec / 1000000;
	if (diff_ms < 180 || diff_ms > 240)
		return -1;

	sc8950_set_aicr(chg_dev, 0);
	mdelay(150);
	sc8950_set_aicr(chg_dev, 800000);
	mdelay(300);
	return 0;
}

static int sc8950_reset_ta(struct charger_device *chg_dev)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);
	union power_supply_propval val = { .intval = 0 };

	dev_info(info->dev, "%s \n", __func__);
	val.intval = 800000;
	power_supply_set_property(info->psy,
				  POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
	val.intval = 50000;
	power_supply_set_property(info->psy,
				  POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	msleep(250);
	val.intval = 2000000;
	power_supply_set_property(info->psy,
				  POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	return 0;
}

static int sc8950_set_dp_function(struct charger_device *chg_dev,
				  enum dpdm_ctrl_status status)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	if ((unsigned int)status >= ARRAY_SIZE(dpdm_ctrl_name))
		return 0;

	dev_info(info->dev, "sc8950 set dp %s\n", dpdm_ctrl_name[status]);
	switch (status) {
	case DPDM_HIZ:
		return sc8950_set_dp(info, 0);
	case DPDM_0V:
		return sc8950_set_dp(info, 1);
	case DPDM_0P6V:
		return sc8950_set_dp(info, 2);
	case DPDM_3P3V:
		return sc8950_set_dp(info, 6);
	default:
		return 0;
	}
}

static int sc8950_set_dm_function(struct charger_device *chg_dev,
				  enum dpdm_ctrl_status status)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	if ((unsigned int)status >= ARRAY_SIZE(dpdm_ctrl_name))
		return 0;

	dev_info(info->dev, "sc8950 set dm %s\n", dpdm_ctrl_name[status]);
	switch (status) {
	case DPDM_HIZ:
		return sc8950_set_dm(info, 0);
	case DPDM_0V:
		return sc8950_set_dm(info, 1);
	case DPDM_0P6V:
		return sc8950_set_dm(info, 2);
	case DPDM_3P3V:
		return sc8950_set_dm(info, 6);
	default:
		return 0;
	}
}

static int sc8950_enable_dcp(struct charger_device *chg_dev, bool en)
{
	struct sc8950_chip *info = charger_dev_get_drvdata(chg_dev);

	dev_info(info->dev, "%s\n", __func__);
	if (en) {
		sc8950_set_dp(info, 2);
		return sc8950_set_dm(info, 2);
	}
	sc8950_set_dp(info, 0);
	return sc8950_set_dm(info, 0);
}

static struct charger_ops sc8950_chg_ops = {
	.plug_in = sc8950_plug_in,
	.plug_out = sc8950_plug_out,
	.enable = sc8950_enable_charging,
	.is_enabled = sc8950_is_charging_enabled,
	.enable_chip = sc8950_enable_powerpath,
	.is_chip_enabled = sc8950_is_powerpath_enabled,
	.get_charging_current = sc8950_get_ichg,
	.set_charging_current = sc8950_set_ichg,
	.get_min_charging_current = sc8950_get_min_ichg,
	.set_constant_voltage = sc8950_set_cv,
	.get_constant_voltage = sc8950_get_cv,
	.get_input_current = sc8950_get_aicr,
	.set_input_current = sc8950_set_aicr,
	.get_min_input_current = sc8950_get_min_aicr,
	.get_eoc_current = sc8950_get_ieoc,
	.set_eoc_current = sc8950_set_ieoc,
	.kick_wdt = sc8950_kick_wdt,
	.event = sc8950_event,
	.send_ta20_current_pattern = sc8950_set_pep20_current_pattern,
	.reset_ta = sc8950_reset_ta,
	.set_mivr = sc8950_set_mivr,
	.get_mivr = sc8950_get_mivr,
	.get_mivr_state = sc8950_get_mivr_state,
	.is_powerpath_enabled = sc8950_is_powerpath_enabled,
	.enable_powerpath = sc8950_enable_powerpath,
	.is_safety_timer_enabled = sc8950_is_safety_timer_enabled,
	.enable_safety_timer = sc8950_enable_safety_timer,
	.enable_termination = sc8950_enable_te,
	.enable_otg = sc8950_enable_otg,
	.set_boost_current_limit = sc8950_set_boost_current_limit,
	.enable_chg_type_det = sc8950_enable_chg_type_det,
	.is_charging_done = sc8950_is_charging_done,
	.set_pe20_efficiency_table = sc8950_set_pep20_efficiency_table,
	.dump_registers = sc8950_dump_registers,
	.set_tc30_gpio_enable = sc8950_tc30_gpio_enable,
	.set_dp = sc8950_set_dp_function,
	.set_dm = sc8950_set_dm_function,
	.enable_hz = sc8950_en_hz,
	.enable_dcp = sc8950_enable_dcp,
};

/* OTG Regulator */
static int sc8950_enable_regulator_otg(struct regulator_dev *rdev)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);

	dev_info(info->dev, "%s\n", __func__);
	return sc8950_enable_otg(info->chg_dev, true);
}

static int sc8950_disable_regulator_otg(struct regulator_dev *rdev)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);

	dev_info(info->dev, "%s\n", __func__);
	return sc8950_enable_otg(info->chg_dev, false);
}

static int sc8950_otg_is_enabled(struct regulator_dev *rdev)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);

	return info->boost_mode;
}

static int sc8950_boost_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);
	u8 regval;

	if (sel <= 3900000)
		regval = 0;
	else if (sel >= 5400000)
		regval = 15;
	else
		regval = (sel - 3800000) / 100000;

	dev_info(info->dev, "%s: sel=%d regval=%d\n", __func__, sel, regval);
	return sc8950_set_boostv(info, regval);
}

static int sc8950_boost_get_voltage_sel(struct regulator_dev *rdev)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);
	u8 val = 0;

	sc8950_read_interface(info, SC8950_REG_0A, &val, 0xf, 4);
	dev_info(info->dev, "%s: val=%d\n", __func__, val);
	return val;
}

static int sc8950_boost_set_current_limit(struct regulator_dev *rdev,
					  int min_uA, int max_uA)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);
	int idx;

	if (min_uA <= 500000)
		idx = 0;
	else if (min_uA <= 750000)
		idx = 1;
	else if (min_uA <= 1200000)
		idx = 2;
	else if (min_uA <= 1400000)
		idx = 3;
	else if (min_uA <= 1650000)
		idx = 4;
	else if (min_uA <= 1875000)
		idx = 5;
	else if (min_uA <= 2150000)
		idx = 6;
	else if (min_uA <= 2450000)
		idx = 7;
	else
		goto out_range;

	if (sc8950_otg_oc_threshold[idx] > (u32)max_uA)
		goto out_range;

	dev_info(info->dev, "%s: select otg_oc = %d\n",
		 __func__, sc8950_otg_oc_threshold[idx]);
	return sc8950_set_iboost(info, idx);

out_range:
	dev_notice(info->dev, "%s: out of current range\n", __func__);
	return -EINVAL;
}

static int sc8950_boost_get_current_limit(struct regulator_dev *rdev)
{
	struct sc8950_chip *info = rdev_get_drvdata(rdev);
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_0A, &val, 0x7, 0);
	if (ret < 0)
		return ret;
	dev_info(info->dev, "%s: regval=%d get otg_oc = %d\n",
		 __func__, val, sc8950_otg_oc_threshold[val]);
	return sc8950_otg_oc_threshold[val];
}

static const struct regulator_ops sc8950_chg_otg_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.set_voltage_sel = sc8950_boost_set_voltage_sel,
	.get_voltage_sel = sc8950_boost_get_voltage_sel,
	.set_current_limit = sc8950_boost_set_current_limit,
	.get_current_limit = sc8950_boost_get_current_limit,
	.enable = sc8950_enable_regulator_otg,
	.disable = sc8950_disable_regulator_otg,
	.is_enabled = sc8950_otg_is_enabled,
};

static const struct regulator_init_data sc8950_vbus_init_data = {
	.constraints = {
		.min_uA = 500000,
		.max_uA = 2450000,
		.valid_ops_mask = REGULATOR_CHANGE_CURRENT | REGULATOR_CHANGE_STATUS,
	},
};

static const struct regulator_desc sc8950_otg_rdesc = {
	.name = "usb-otg-vbus",
	.of_match = "usb-otg-vbus",
	.ops = &sc8950_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
	.n_voltages = 15,
	.min_uV = 3900000,
	.uV_step = 100000,
};

/* Power supply callbacks */
static int sc8950_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sc8950_chip *info = power_supply_get_drvdata(psy);
	u8 stat = 0, vbus_gd = 0;
	bool chg_en = false;
	int online;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		sc8950_read_interface(info, SC8950_REG_0B, &stat, 0x3, 3);
		if (stat == 0) {
			sc8950_read_interface(info, SC8950_REG_11, &vbus_gd, 0x1, 7);
			stat = vbus_gd ? 2 : 0;
		}
		online = atomic_read(&info->online);
		sc8950_is_charging_enabled(info->chg_dev, &chg_en);
		dev_info(info->dev, " %s stat =%d,online =%d,chg_en = %d\n",
			 "sc8950_get_ic_stat", stat, online, chg_en);
		switch (stat) {
		case 0:
			val->intval = chg_en ? POWER_SUPPLY_STATUS_DISCHARGING :
					       POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case 1:
		case 2:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case 3:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		}
		return 0;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = atomic_read(&info->online);
		dev_err(info->dev, "%s: online = %d\n", __func__, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		return 0;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return sc8950_get_ichg(info->chg_dev, (u32 *)&val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return sc8950_get_cv(info->chg_dev, (u32 *)&val->intval);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return sc8950_get_aicr(info->chg_dev, (u32 *)&val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return sc8950_get_ieoc(info->chg_dev, (u32 *)&val->intval);
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = info->psy_desc.type;
		dev_err(info->dev, "%s: psy desc type = %d\n", __func__, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = info->psy_usb_type;
		dev_err(info->dev, "%s: psy usb type = %d\n", __func__, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Southchip";
		return 0;
	default:
		return -ENODATA;
	}
}

static int sc8950_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct sc8950_chip *info = power_supply_get_drvdata(psy);

	dev_info(info->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		dev_notice(info->dev, "attach=%d\n", val->intval != 0);
		return sc8950_enable_chg_type_det(info->chg_dev, val->intval != 0);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return sc8950_set_ichg(info->chg_dev, val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return sc8950_set_cv(info->chg_dev, val->intval);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return sc8950_set_aicr(info->chg_dev, val->intval);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return sc8950_set_mivr(info->chg_dev, val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return sc8950_set_ieoc(info->chg_dev, val->intval);
	default:
		return -EINVAL;
	}
}

static int sc8950_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	return psp == POWER_SUPPLY_PROP_ONLINE;
}

static const struct power_supply_desc sc8950_charger_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sc8950_charger_usb_types,
	.properties = sc8950_charger_properties,
	.num_properties = ARRAY_SIZE(sc8950_charger_properties),
	.get_property = sc8950_charger_get_property,
	.set_property = sc8950_charger_set_property,
	.property_is_writeable = sc8950_charger_property_is_writeable,
};

/* Sysfs register debug */
static ssize_t sc8950_show_registers(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sc8950_chip *info = dev_get_drvdata(dev);
	u8 reg, val = 0;
	char tmpbuf[300];
	int idx, len;

	memset(tmpbuf, 0, sizeof(tmpbuf));
	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8950");
	for (reg = 0; reg < SC8950_REG_NUM; reg++) {
		if (sc8950_read_byte(info, reg, &val) == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2X] = 0x%.2x\n", reg, val);
			memcpy(buf + idx, tmpbuf, len);
			idx += len;
		}
	}
	return idx;
}

static ssize_t sc8950_store_register(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sc8950_chip *info = dev_get_drvdata(dev);
	unsigned int reg = 0, val = 0;

	if (sscanf(buf, "%x %x", &reg, &val) == 2)
		sc8950_write_byte(info, reg, val);
	return count;
}

static DEVICE_ATTR(sc8950_registers_debug, 0664,
		   sc8950_show_registers, sc8950_store_register);

static int sc8950_parse_dt(struct sc8950_chip *info, struct device_node *np)
{
	int ret;

	dev_info(info->dev, "%s\n", __func__);
	if (!np) {
		dev_info(info->dev, "%s: no of node\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_string(np, "chg_name", &info->chg_name);
	if (ret < 0) {
		info->chg_name = "primary_chg";
		dev_err(info->dev, "%s no chg_name(%d)\n", __func__, ret);
	}

	ret = of_property_read_string(np, "chg_alias_name",
				      &info->chg_props.alias_name);
	if (ret < 0) {
		info->chg_props.alias_name = "sc8950";
		pr_info("%s: no alias name\n", __func__);
	}

	info->intr_gpio = of_get_named_gpio(np, "sc8950,intr_gpio", 0);
	return 0;
}

static int sc8950_check_devinfo(struct sc8950_chip *info)
{
	u8 val = 0;
	int ret;

	ret = sc8950_read_interface(info, SC8950_REG_14, &val, 0x7, 3);
	if (ret < 0) {
		dev_notice(info->dev, "%s get_pd fail(%d)\n", __func__, ret);
		return -ENODEV;
	}
	if (val != 2) {
		info->part_no = -1;
		return -ENODEV;
	}
	info->part_no = 2;
	dev_info(info->dev, "%s chip id is 0x%x\n", __func__, val);
	return 0;
}

static void sc8950_hw_init(struct sc8950_chip *info)
{
	u8 tmp = 0;

	dev_info(info->dev, "%s", __func__);
	sc8950_set_rst(info, 1);
	sc8950_set_ico(info, 0);
	sc8950_set_hvdcp(info, 0);
	sc8950_set_autodpdm(info, 0);
	sc8950_set_hiz(info, 0);
	sc8950_set_force_vindpm(info, 0x2d);
	sc8950_set_wdt_rst(info, 1);
	sc8950_set_vsys_min(info, 5);
	sc8950_set_iprechg(info, 7);
	sc8950_set_iterm(info, 3);
	sc8950_set_vreg(info, 0x23);
	sc8950_set_statdis(info, 0);
	sc8950_set_vbatlow_otg(info, 0);
	sc8950_set_vrechg(info, 0);
	sc8950_set_en_term(info, 1);
	sc8950_set_watchdog_cfg(info, 0);
	sc8950_set_en_timer(info, 1);
	sc8950_set_tchg(info, 2);
	sc8950_set_batfet_reset_en(info, 0);

	if (sc8950_read_byte(info, SC8950_REG_HIDDEN, &tmp) < 0)
		sc8950_key_toggle(info);
	sc8950_update_bits(info, SC8950_REG_HIDDEN, 2, 0x3, 1);
	sc8950_key_toggle(info);

	sc8950_set_chg_thermal(info, 3);
	sc8950_set_boostv(info, 0xe);
	sc8950_set_en_charge(info, 0);
	info->boost_mode = 0;
}

static int sc8950_register_interrupt(struct sc8950_chip *info)
{
	char *name;
	int ret;

	if (info->intr_gpio == -1) {
		pr_err("%s failed!no intr_gpio!\n", __func__);
		return 0;
	}

	dev_info(info->dev, "%s\n", __func__);
	name = devm_kasprintf(info->dev, GFP_KERNEL, "sc8950_intr_gpio.%s",
			      dev_name(info->dev));
	ret = devm_gpio_request_one(info->dev, info->intr_gpio, GPIOF_IN, name);
	if (ret < 0) {
		dev_notice(info->dev, "%s gpio request fail(%d)\n", __func__, ret);
		return ret;
	}

	info->irq = gpio_to_irq(info->intr_gpio);
	dev_info(info->dev, "%s irq = %d\n", __func__, info->irq);

	name = devm_kasprintf(info->dev, GFP_KERNEL, "sc8950_irq.%s",
			      dev_name(info->dev));
	ret = devm_request_threaded_irq(info->dev, info->irq, NULL,
					sc8950_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					name, info);
	if (ret < 0) {
		dev_notice(info->dev, "%s request threaded irq fail(%d)\n",
			   __func__, ret);
		return ret;
	}
	return 0;
}

static int sc8950_init_regulator(struct sc8950_chip *info)
{
	struct regulator_config config = {};

	dev_info(info->dev, "%s\n", __func__);
	config.dev = info->dev;
	config.init_data = &sc8950_vbus_init_data;
	config.driver_data = info;

	info->otg_rdev = devm_regulator_register(info->dev, &sc8950_otg_rdesc, &config);
	if (IS_ERR(info->otg_rdev)) {
		int ret = PTR_ERR(info->otg_rdev);

		dev_info(info->dev, "%s fail (%d)!\n", __func__, ret);
		return ret;
	}
	dev_info(info->dev, "%s succesfully!\n", __func__);
	return 0;
}

static void sc8950_init_psy(struct sc8950_chip *info)
{
	struct power_supply_config psy_cfg = {};

	dev_info(info->dev, "%s\n", __func__);
	memcpy(&info->psy_desc, &sc8950_charger_desc, sizeof(info->psy_desc));
	info->psy_desc.name = "charger";
	info->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	psy_cfg.of_node = info->dev->of_node;
	psy_cfg.drv_data = info;
	psy_cfg.supplied_to = sc8950_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sc8950_charger_supplied_to);

	info->psy = devm_power_supply_register(info->dev, &info->psy_desc, &psy_cfg);
}

static int sc8950_probe(struct i2c_client *client)
{
	struct sc8950_chip *info;
	int ret;

	dev_info(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_info(&client->dev, "%s alloc sc8950_chip failed!\n", __func__);
		return -ENOMEM;
	}

	info->client = client;
	info->dev = &client->dev;
	i2c_set_clientdata(client, info);

	mutex_init(&info->sc8950_i2c_access);
	mutex_init(&info->attach_lock);
	atomic_set(&info->online, 0);
	atomic_set(&info->attach, 0);

	ret = sc8950_parse_dt(info, client->dev.of_node);
	if (ret < 0) {
		dev_err(&client->dev, "%s parse dt failed!\n", __func__);
		devm_kfree(info->dev, info);
		return 0;
	}

	ret = sc8950_check_devinfo(info);
	if (ret < 0) {
		dev_err(&client->dev, "%s sc8950 ic is not exist!\n", __func__);
		devm_kfree(info->dev, info);
		return -ENODEV;
	}

	sc8950_hw_init(info);

	info->chg_dev = charger_device_register(info->chg_name, info->dev, info,
						&sc8950_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev) || !info->chg_dev->dev.class) {
		pr_info("%s: register charger device  failed\n", __func__);
		devm_kfree(info->dev, info);
		return -EPROBE_DEFER;
	}

	info->irq_wq = create_singlethread_workqueue("sc8950_primary");
	if (!info->irq_wq)
		dev_info(info->dev, "%s create_singlethread_workqueue failed!\n", __func__);
	else
		INIT_WORK(&info->irq_work, sc8950_work_handler);

	ret = sc8950_register_interrupt(info);
	if (ret < 0) {
		dev_err(info->dev, "%s register irq fail(%d)\n", __func__, ret);
		goto err_unreg;
	}

	ret = sc8950_init_regulator(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s regulator register fail\n", __func__);
		goto err_unreg;
	}

	sc8950_init_psy(info);
	device_create_file(info->dev, &dev_attr_sc8950_registers_debug);
	INIT_WORK(&info->bc12_work, sc8950_bc12_detect);

	__sc8950_dump_registers(info);
	dev_err(info->dev, "sc8950 probe successfully!\n");
	return 0;

err_unreg:
	charger_device_unregister(info->chg_dev);
	devm_kfree(info->dev, info);
	return ret;
}

static void sc8950_remove(struct i2c_client *client)
{
	struct sc8950_chip *info = i2c_get_clientdata(client);

	if (!info)
		return;
	if (info->psy)
		power_supply_put(info->psy);
	charger_device_unregister(info->chg_dev);
	dev_info(info->dev, "%s end\n", __func__);
}

static void sc8950_shutdown(struct i2c_client *client)
{
	struct sc8950_chip *info = i2c_get_clientdata(client);

	if (!info)
		return;
	sc8950_set_rst(info, 1);
	atomic_set(&info->online, 0);
	i2c_set_clientdata(client, NULL);
	dev_info(info->dev, "%s end\n", __func__);
}

static int sc8950_suspend(struct device *dev)
{
	return 0;
}

static int sc8950_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(sc8950_pm_ops, sc8950_suspend, sc8950_resume);

static const struct of_device_id sc8950_of_device_id[] = {
	{ .compatible = "southchip,sc8950" },
	{ }
};
MODULE_DEVICE_TABLE(of, sc8950_of_device_id);

static struct i2c_driver sc8950_i2c_driver = {
	.driver = {
		.name = "sc8950",
		.owner = THIS_MODULE,
		.of_match_table = sc8950_of_device_id,
		.pm = &sc8950_pm_ops,
	},
	.probe = sc8950_probe,
	.remove = sc8950_remove,
	.shutdown = sc8950_shutdown,
};
module_i2c_driver(sc8950_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SC8950 Charger Driver");
