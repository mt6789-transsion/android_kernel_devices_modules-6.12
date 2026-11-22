// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * Halo Micro HL7138 / HL7139 2:1 / 1:1 Switched-Capacitor Charge Pump Driver.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/string.h>

#include <charger_class.h>

#define HL7138_REG_DEVICE_ID		0x00
#define HL7138_REG_INT			0x01
#define HL7138_REG_INT_MSK		0x02
#define HL7138_REG_INT_STS_A		0x03
#define HL7138_REG_INT_STS_B		0x04
#define HL7138_REG_STATUS_A		0x05
#define HL7138_REG_STATUS_B		0x06
#define HL7138_REG_VBAT_REG		0x08
#define HL7138_REG_IBAT_REG		0x0a
#define HL7138_REG_VBUS_OVP		0x0b
#define HL7138_REG_IIN_OCP		0x0e
#define HL7138_REG_IIN_REG		0x10
#define HL7138_REG_VBAT_IBAT_REG	0x11
#define HL7138_REG_CTRL0		0x12
#define HL7138_REG_CTRL1		0x13
#define HL7138_REG_CTRL2		0x14
#define HL7138_REG_CTRL3		0x15
#define HL7138_REG_TRACK_OV_UV		0x16
#define HL7138_REG_DPDM_CFG0		0x20
#define HL7138_REG_DPDM_CFG1		0x21
#define HL7138_REG_ADC_CTRL0		0x40
#define HL7138_REG_ADC_CTRL1		0x41
#define HL7138_REG_ADC_VBUS		0x42
#define HL7138_REG_ADC_IBUS		0x44
#define HL7138_REG_ADC_VBAT		0x46
#define HL7138_REG_ADC_IBAT		0x48
#define HL7138_REG_ADC_VTS		0x4a
#define HL7138_REG_ADC_VOUT		0x4c
#define HL7138_REG_ADC_TDIE		0x4e
#define HL7138_REG_CHG_STS		0xa7

#define HL7138_DEVID			0x03
#define HL7139_DEVID			0x0a

#define HL7138_ADC_EN_MASK		BIT(0)
#define HL7138_CHG_EN_MASK		BIT(7)
#define HL7138_WDT_DIS_MASK		BIT(3)
#define HL7138_VBUS_LOWERR_MASK		BIT(4)

enum hl7138_adc_channel {
	HL7138_ADC_VBUS = 0,
	HL7138_ADC_IBUS,
	HL7138_ADC_VBAT,
	HL7138_ADC_IBAT,
	HL7138_ADC_VTS,
	HL7138_ADC_VOUT,
	HL7138_ADC_TDIE,
	HL7138_ADC_MAX,
};

enum hl7138_notify {
	HL7138_NOTIFY_IBUSUCPF = 0,
	HL7138_NOTIFY_VBUSOVPALM,
	HL7138_NOTIFY_VBATOVPALM,
	HL7138_NOTIFY_IBUSOCP,
	HL7138_NOTIFY_VBUSOVP,
	HL7138_NOTIFY_IBATOCP,
	HL7138_NOTIFY_VBATOVP,
	HL7138_NOTIFY_VOUTOVP,
	HL7138_NOTIFY_VDROVP,
	HL7138_NOTIFY_MAX,
};

struct hl7138_desc {
	const char *chg_name;
	u32 rm_res;
	u32 rm_offset;
	u32 vbatovp;
	u32 ibatocp;
	u32 vbusovp;
	u32 ibusocp;
	u32 wdt;
	u32 gpp_conf;
	bool state_chg_i_masked;
	bool reg_i_masked;
	bool ts_temp_i_masked;
	bool v_ok_i_masked;
	bool cur_i_masked;
	bool short_i_masked;
	bool wdog_i_masked;
	bool protocol_i_masked;
	bool charge_mode;
	bool vbat_reg_dis;
	bool ibat_reg_dis;
	bool iin_reg_dis;
	bool track_ov_dis;
	bool track_uv_dis;
	bool vbatovp_dis;
	bool ibatocp_dis;
	bool ibusocp_dis;
	bool wdt_dis;
	bool voutovp_dis;
	bool ibusadc_dis;
	bool vbusadc_dis;
	bool voutadc_dis;
	bool vbatadc_dis;
	bool ibatadc_dis;
	bool tdieadc_dis;
};

struct hl7138_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex io_lock;
	struct mutex adc_lock;
	struct mutex stat_lock;
	struct mutex notify_lock;
	struct hl7138_desc *desc;
	struct charger_device *chg_dev;
	struct charger_properties chg_prop;
	struct gpio_desc *irq_gpio;
	int irq;
	struct task_struct *notify_task;
	wait_queue_head_t wq;
	struct work_struct state_work;
	u32 notify;
	u8 rev;
	u8 type;
	int chip_id;
	bool force_adc_en;
	bool stop_thread;
	u8 sf_regs[5];
};

static const struct hl7138_desc hl7138_desc_defval = {
	.chg_name = "divider_charger",
	.vbatovp = 4350000,
	.ibatocp = 8100000,
	.vbusovp = 8900000,
	.ibusocp = 4250000,
	.wdt = 500000,
	.gpp_conf = 1,
	.state_chg_i_masked = true,
	.reg_i_masked = true,
	.ts_temp_i_masked = true,
	.v_ok_i_masked = true,
	.cur_i_masked = true,
	.short_i_masked = true,
	.wdog_i_masked = true,
	.protocol_i_masked = true,
	.vbat_reg_dis = true,
	.ibat_reg_dis = true,
	.iin_reg_dis = true,
	.wdt_dis = true,
};

static const u8 hl7138_reg_sf[5] = {
	HL7138_REG_INT,
	HL7138_REG_INT_STS_A,
	HL7138_REG_INT_STS_B,
	HL7138_REG_STATUS_A,
	HL7138_REG_STATUS_B,
};

static const u32 hl7138_adc_accuracy_tbl[HL7138_ADC_MAX] = {
	150000,	/* Vbus */
	150000,	/* Ibus */
	35000,	/* Vbat */
	150000,	/* Ibat */
	4,	/* VTS */
	35000,	/* Vout */
	4,	/* TDie */
};

static const u8 hl7138_adc_reg[HL7138_ADC_MAX] = {
	HL7138_REG_ADC_VBUS,
	HL7138_REG_ADC_IBUS,
	HL7138_REG_ADC_VBAT,
	HL7138_REG_ADC_IBAT,
	HL7138_REG_ADC_VTS,
	HL7138_REG_ADC_VOUT,
	HL7138_REG_ADC_TDIE,
};

static const char *const hl7138_adc_name[HL7138_ADC_MAX] = {
	"Vbus", "Ibus", "Vbat", "Ibat", "VTS", "Vout", "TDie",
};

static const char *const hl7138_type_name[] = {
	"standalone", "slave", "master", "unknown",
};

/* I2C helpers with retry + wakeup guard */
static int __hl7138_i2c_read_block(struct hl7138_chip *chip, u8 reg, u8 len, u8 *data)
{
	int i, ret = 0;

	for (i = 0; i < 5; i++) {
		pm_stay_awake(chip->dev);
		mutex_lock(&chip->io_lock);
		ret = i2c_smbus_read_i2c_block_data(chip->client, reg, len, data);
		mutex_unlock(&chip->io_lock);
		pm_relax(chip->dev);
		if (ret >= 0)
			return 0;
		usleep_range(10, 15);
	}
	dev_err(chip->dev, "%s I2CR[0x%02X] fail\n", "__hl7138_i2c_read8", reg);
	return ret;
}

static int __hl7138_i2c_write_block(struct hl7138_chip *chip, u8 reg, u8 len, const u8 *data)
{
	int i, ret = 0;

	for (i = 0; i < 5; i++) {
		pm_stay_awake(chip->dev);
		mutex_lock(&chip->io_lock);
		ret = i2c_smbus_write_i2c_block_data(chip->client, reg, len, data);
		mutex_unlock(&chip->io_lock);
		pm_relax(chip->dev);
		if (ret >= 0)
			return 0;
		usleep_range(10, 15);
	}
	dev_err(chip->dev, "%s I2CW[0x%02X] = 0x%02X fail\n",
		"__hl7138_i2c_write8", reg, data[0]);
	return ret;
}

static int hl7138_i2c_read8(struct hl7138_chip *chip, u8 reg, u8 *val)
{
	return __hl7138_i2c_read_block(chip, reg, 1, val);
}

static int hl7138_i2c_write8(struct hl7138_chip *chip, u8 reg, u8 val)
{
	return __hl7138_i2c_write_block(chip, reg, 1, &val);
}

static int hl7138_update_bits(struct hl7138_chip *chip, u8 reg, u8 mask, u8 val)
{
	u8 tmp = 0;
	int ret;

	ret = hl7138_i2c_read8(chip, reg, &tmp);
	if (ret < 0)
		return ret;
	tmp = (tmp & ~mask) | (val & mask);
	return hl7138_i2c_write8(chip, reg, tmp);
}

static int hl7138_set_bits(struct hl7138_chip *chip, u8 reg, u8 mask)
{
	return hl7138_update_bits(chip, reg, mask, mask);
}

static int hl7138_clr_bits(struct hl7138_chip *chip, u8 reg, u8 mask)
{
	return hl7138_update_bits(chip, reg, mask, 0);
}

/* Threshold conversion helpers */
static u8 hl7138_vbatovp_toreg(u32 uV)
{
	if (uV <= 4000000)
		return 0;
	if (uV >= 4600000)
		return 60;
	return (uV - 4000000) / 10000;
}

static u8 hl7138_ibatocp_toreg(u32 uA)
{
	if (uA <= 2000000)
		return 0;
	if (uA >= 6600000)
		return 46;
	return (uA - 1900000) / 100000;
}

static u8 hl7138_vbusovp_toreg(u32 uV)
{
	if (uV <= 4000000)
		return 0;
	if (uV >= 19000000)
		return 15;
	return (uV - 3000000) / 1000000;
}

static u8 hl7138_ibusocp_toreg(u32 uA)
{
	if (uA <= 1000000)
		return 0;
	if (uA >= 3500000)
		return 50;
	return (uA - 950000) / 50000;
}

static u8 hl7138_wdt_toreg(u32 us)
{
	if (us < 500000)
		return 0;
	if (us < 1000000)
		return 1;
	if (us < 2000000)
		return 2;
	if (us < 5000000)
		return 3;
	if (us < 10000000)
		return 4;
	if (us < 20000000)
		return 5;
	if (us < 40000000)
		return 6;
	return 7;
}

struct hl7138_dtprop_bool {
	const char *name;
	size_t offset;
	u8 reg;
	u8 shift;
	u8 mask;
};

struct hl7138_dtprop_u32 {
	const char *name;
	size_t offset;
	u8 reg;
	u8 shift;
	u8 mask;
	u8 (*toreg)(u32 val);
};

static const struct hl7138_dtprop_bool hl7138_dtprops_bool[] = {
	{ "state_chg_i_masked", offsetof(struct hl7138_desc, state_chg_i_masked), 0x02, 7, 0x80 },
	{ "reg_i_masked",       offsetof(struct hl7138_desc, reg_i_masked),       0x02, 6, 0x40 },
	{ "ts_temp_i_masked",   offsetof(struct hl7138_desc, ts_temp_i_masked),   0x02, 5, 0x20 },
	{ "v_ok_i_masked",      offsetof(struct hl7138_desc, v_ok_i_masked),      0x02, 4, 0x10 },
	{ "cur_i_masked",       offsetof(struct hl7138_desc, cur_i_masked),       0x02, 3, 0x08 },
	{ "short_i_masked",     offsetof(struct hl7138_desc, short_i_masked),     0x02, 2, 0x04 },
	{ "wdog_i_masked",      offsetof(struct hl7138_desc, wdog_i_masked),      0x02, 1, 0x02 },
	{ "protocol_i_masked",  offsetof(struct hl7138_desc, protocol_i_masked),  0x02, 0, 0x01 },
	{ "charge_mode",        offsetof(struct hl7138_desc, charge_mode),        0x15, 7, 0x80 },
	{ "vbat_reg_dis",       offsetof(struct hl7138_desc, vbat_reg_dis),       0x11, 7, 0x80 },
	{ "ibat_reg_dis",       offsetof(struct hl7138_desc, ibat_reg_dis),       0x11, 6, 0x40 },
	{ "iin_reg_dis",        offsetof(struct hl7138_desc, iin_reg_dis),        0x10, 6, 0x40 },
	{ "vbatovp_dis",        offsetof(struct hl7138_desc, vbatovp_dis),        0x08, 7, 0x80 },
	{ "ibatocp_dis",        offsetof(struct hl7138_desc, ibatocp_dis),        0x0a, 7, 0x80 },
	{ "ibusocp_dis",        offsetof(struct hl7138_desc, ibusocp_dis),        0x0e, 7, 0x80 },
	{ "track_ov_dis",       offsetof(struct hl7138_desc, track_ov_dis),       0x16, 7, 0x80 },
	{ "track_uv_dis",       offsetof(struct hl7138_desc, track_uv_dis),       0x16, 6, 0x40 },
	{ "wdt_dis",            offsetof(struct hl7138_desc, wdt_dis),            0x14, 3, 0x08 },
	{ "voutovp_dis",        offsetof(struct hl7138_desc, voutovp_dis),        0x13, 5, 0x20 },
	{ "ibusadc_dis",        offsetof(struct hl7138_desc, ibusadc_dis),        0x41, 6, 0x40 },
	{ "tdieadc_dis",        offsetof(struct hl7138_desc, tdieadc_dis),        0x41, 2, 0x04 },
	{ "ibatadc_dis",        offsetof(struct hl7138_desc, ibatadc_dis),        0x41, 4, 0x10 },
	{ "vbatadc_dis",        offsetof(struct hl7138_desc, vbatadc_dis),        0x41, 5, 0x20 },
	{ "voutadc_dis",        offsetof(struct hl7138_desc, voutadc_dis),        0x41, 1, 0x02 },
	{ "vbusadc_dis",        offsetof(struct hl7138_desc, vbusadc_dis),        0x41, 7, 0x80 },
};

static const struct hl7138_dtprop_u32 hl7138_dtprops_u32[] = {
	{ "vbatovp",  offsetof(struct hl7138_desc, vbatovp),  0x08, 0, 0x3f, hl7138_vbatovp_toreg },
	{ "ibatocp",  offsetof(struct hl7138_desc, ibatocp),  0x0a, 0, 0x3f, hl7138_ibatocp_toreg },
	{ "vbusovp",  offsetof(struct hl7138_desc, vbusovp),  0x0b, 0, 0x0f, hl7138_vbusovp_toreg },
	{ "ibusocp",  offsetof(struct hl7138_desc, ibusocp),  0x0e, 0, 0x3f, hl7138_ibusocp_toreg },
	{ "wdt",      offsetof(struct hl7138_desc, wdt),      0x14, 0, 0x07, hl7138_wdt_toreg },
	{ "gpp_conf", offsetof(struct hl7138_desc, gpp_conf), 0x15, 0, 0x03, NULL },
};

/* IRQ notification helpers */
static void hl7138_set_notify(struct hl7138_chip *chip, enum hl7138_notify evt)
{
	mutex_lock(&chip->notify_lock);
	chip->notify |= BIT(evt);
	mutex_unlock(&chip->notify_lock);
	wake_up_interruptible(&chip->wq);
}

static int hl7138_state_chg_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_reg_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_ts_temp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_v_not_ok_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_vin_ovp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_VBUSOVP);
	return 0;
}

static int hl7138_vin_uvlo_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_track_ov_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_track_uv_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_vbat_ovp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_VBATOVP);
	return 0;
}

static int hl7138_vout_ovp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_VOUTOVP);
	return 0;
}

static int hl7138_pmic_qual_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_vubs_uv_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_cur_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_iin_ocp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_IBUSOCP);
	return 0;
}

static int hl7138_ibat_ocp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_IBATOCP);
	return 0;
}

static int hl7138_iin_ucp_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	hl7138_set_notify(chip, HL7138_NOTIFY_IBUSUCPF);
	return 0;
}

static int hl7138_short_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_fet_short_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_cfly_short_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_wdog_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_dev_mode_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int hl7138_thsd_sts_irq_handler(struct hl7138_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

struct hl7138_irq_map {
	const char *name;
	int (*hdlr)(struct hl7138_chip *chip);
	u8 reg_idx;
	u8 mask;
};

static const struct hl7138_irq_map hl7138_irq_map_tbl[] = {
	{ "state_chg_sts",  hl7138_state_chg_sts_irq_handler,  0, 0x80 },
	{ "reg_sts",        hl7138_reg_sts_irq_handler,        0, 0x40 },
	{ "ts_temp_sts",    hl7138_ts_temp_sts_irq_handler,    0, 0x20 },
	{ "v_not_ok_sts",   hl7138_v_not_ok_sts_irq_handler,   0, 0x10 },
	{ "vin_ovp_sts",    hl7138_vin_ovp_sts_irq_handler,    3, 0x80 },
	{ "vin_uvlo_sts",   hl7138_vin_uvlo_sts_irq_handler,   3, 0x40 },
	{ "track_ov_sts",   hl7138_track_ov_sts_irq_handler,   3, 0x20 },
	{ "track_uv_sts",   hl7138_track_uv_sts_irq_handler,   3, 0x10 },
	{ "vbat_ovp_sts",   hl7138_vbat_ovp_sts_irq_handler,   3, 0x08 },
	{ "vout_ovp_sts",   hl7138_vout_ovp_sts_irq_handler,   3, 0x04 },
	{ "pmic_qual_sts",  hl7138_pmic_qual_sts_irq_handler,  3, 0x02 },
	{ "vubs_uv_sts",    hl7138_vubs_uv_sts_irq_handler,    3, 0x01 },
	{ "cur_sts",        hl7138_cur_sts_irq_handler,        0, 0x08 },
	{ "iin_ocp_sts",    hl7138_iin_ocp_sts_irq_handler,    4, 0x80 },
	{ "ibat_ocp_sts",   hl7138_ibat_ocp_sts_irq_handler,   4, 0x40 },
	{ "iin_ucp_sts",    hl7138_iin_ucp_sts_irq_handler,    4, 0x20 },
	{ "short_sts",      hl7138_short_sts_irq_handler,      0, 0x04 },
	{ "fet_short_sts",  hl7138_fet_short_sts_irq_handler,  4, 0x10 },
	{ "cfly_short_sts", hl7138_cfly_short_sts_irq_handler, 4, 0x08 },
	{ "wdog_sts",       hl7138_wdog_sts_irq_handler,       0, 0x02 },
	{ "dev_mode_sts",   hl7138_dev_mode_sts_irq_handler,   4, 0x04 },
	{ "thsd_sts",       hl7138_thsd_sts_irq_handler,       4, 0x02 },
};

static int __hl7138_update_status(struct hl7138_chip *chip)
{
	int i, ret;

	mutex_lock(&chip->stat_lock);
	for (i = 0; i < ARRAY_SIZE(hl7138_reg_sf); i++) {
		ret = hl7138_i2c_read8(chip, hl7138_reg_sf[i], &chip->sf_regs[i]);
		if (ret < 0)
			break;
	}
	for (i = 0; i < ARRAY_SIZE(hl7138_irq_map_tbl); i++) {
		const struct hl7138_irq_map *m = &hl7138_irq_map_tbl[i];

		if ((chip->sf_regs[m->reg_idx] & m->mask) && m->hdlr)
			m->hdlr(chip);
	}
	mutex_unlock(&chip->stat_lock);
	return 0;
}

static void hl7138_state_update_handler(struct work_struct *work)
{
	struct hl7138_chip *chip = container_of(work, struct hl7138_chip, state_work);

	__hl7138_update_status(chip);
}

static irqreturn_t hl7138_irq_handler(int irq, void *data)
{
	struct hl7138_chip *chip = data;

	queue_work(system_wq, &chip->state_work);
	return IRQ_HANDLED;
}

static int hl7138_notify_task_threadfn(void *data)
{
	struct hl7138_chip *chip = data;
	u32 notify;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->wq,
					 chip->notify != 0 || kthread_should_stop());
		if (kthread_should_stop())
			break;

		pm_stay_awake(chip->dev);
		mutex_lock(&chip->notify_lock);
		notify = chip->notify;
		chip->notify = 0;
		mutex_unlock(&chip->notify_lock);

		if (notify & BIT(HL7138_NOTIFY_IBUSUCPF))
			charger_dev_notify(chip->chg_dev, 9);
		if (notify & BIT(HL7138_NOTIFY_VBUSOVPALM))
			charger_dev_notify(chip->chg_dev, 8);
		if (notify & BIT(HL7138_NOTIFY_VBATOVPALM))
			charger_dev_notify(chip->chg_dev, 0);
		if (notify & BIT(HL7138_NOTIFY_IBUSOCP))
			charger_dev_notify(chip->chg_dev, 7);
		if (notify & BIT(HL7138_NOTIFY_VBUSOVP))
			charger_dev_notify(chip->chg_dev, 1);
		if (notify & BIT(HL7138_NOTIFY_IBATOCP))
			charger_dev_notify(chip->chg_dev, 10);
		if (notify & BIT(HL7138_NOTIFY_VBATOVP))
			charger_dev_notify(chip->chg_dev, 11);

		pm_relax(chip->dev);
	}
	return 0;
}

static int __hl7138_init_chip(struct hl7138_chip *chip)
{
	struct hl7138_desc *desc = chip->desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(hl7138_dtprops_bool); i++) {
		const struct hl7138_dtprop_bool *p = &hl7138_dtprops_bool[i];
		bool val = *(bool *)((u8 *)desc + p->offset);

		ret = hl7138_update_bits(chip, p->reg, p->mask,
					 val ? (1 << p->shift) : 0);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(hl7138_dtprops_u32); i++) {
		const struct hl7138_dtprop_u32 *p = &hl7138_dtprops_u32[i];
		u32 val = *(u32 *)((u8 *)desc + p->offset);
		u8 regv = p->toreg ? p->toreg(val) : (u8)val;

		ret = hl7138_update_bits(chip, p->reg, p->mask, regv << p->shift);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int hl7138_internal_voltage_protection(struct hl7138_chip *chip)
{
	u8 val = 0;

	return hl7138_i2c_read8(chip, HL7138_REG_STATUS_A, &val);
}

/* Charger class operations */
static int hl7138_plug_out(struct charger_device *chg_dev)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);

	dev_info(chip->dev, "%s\n", __func__);
	hl7138_i2c_write8(chip, HL7138_REG_INT_MSK, 0xc0);
	hl7138_i2c_write8(chip, HL7138_REG_ADC_CTRL0, 0x00);
	return 0;
}

static int hl7138_enable_chg(struct charger_device *chg_dev, bool en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	dev_info(chip->dev, "%s %d\n", __func__, en);
	if (en) {
		ret = hl7138_set_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
		if (ret < 0)
			return ret;
		hl7138_clr_bits(chip, HL7138_REG_CTRL2, HL7138_WDT_DIS_MASK);
		hl7138_internal_voltage_protection(chip);
		ret = hl7138_set_bits(chip, HL7138_REG_CTRL0, HL7138_CHG_EN_MASK);
	} else {
		ret = hl7138_clr_bits(chip, HL7138_REG_CTRL0, HL7138_CHG_EN_MASK);
		hl7138_clr_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
		hl7138_set_bits(chip, HL7138_REG_CTRL2, HL7138_WDT_DIS_MASK);
	}

	if (chip->chip_id == HL7138_DEVID) {
		if (hl7138_i2c_read8(chip, HL7138_REG_CHG_STS, &val) == 0)
			dev_info(chip->dev, "%s reg[0xa7]:%x\n", __func__, val);
	}
	return ret;
}

static int hl7138_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	if (chip->chip_id == HL7138_DEVID) {
		ret = hl7138_i2c_read8(chip, HL7138_REG_CHG_STS, &val);
		if (ret < 0)
			return ret;
		*en = !!(val & HL7138_CHG_EN_MASK);
	} else {
		ret = hl7138_i2c_read8(chip, HL7138_REG_CTRL0, &val);
		if (ret < 0)
			return ret;
		*en = !!(val & HL7138_CHG_EN_MASK);
	}
	return 0;
}

static int hl7138_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 regv = hl7138_ibusocp_toreg(uA);

	return hl7138_update_bits(chip, HL7138_REG_IIN_OCP, 0x3f, regv);
}

static int hl7138_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 regv = hl7138_vbusovp_toreg(uV);

	return hl7138_update_bits(chip, HL7138_REG_VBUS_OVP, 0x0f, regv);
}

static int hl7138_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 regv = hl7138_ibatocp_toreg(uA);

	return hl7138_update_bits(chip, HL7138_REG_IBAT_REG, 0x3f, regv);
}

static int hl7138_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 regv = hl7138_vbatovp_toreg(uV);

	return hl7138_update_bits(chip, HL7138_REG_VBAT_REG, 0x3f, regv);
}

static int hl7138_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	return 0;
}

static int hl7138_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	return 0;
}

static int hl7138_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;
	int ret;

	mutex_lock(&chip->adc_lock);
	hl7138_set_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
	usleep_range(12000, 15000);
	ret = hl7138_i2c_read8(chip, HL7138_REG_STATUS_A, &val);
	if (!chip->force_adc_en)
		hl7138_clr_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
	mutex_unlock(&chip->adc_lock);

	if (ret < 0)
		return ret;
	*err = !!(val & HL7138_VBUS_LOWERR_MASK);
	return 0;
}

static int hl7138_init_chip(struct charger_device *chg_dev)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);

	return __hl7138_init_chip(chip);
}

static int hl7138_to_adc_chan(enum adc_channel chan)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		return HL7138_ADC_VBUS;
	case ADC_CHANNEL_IBUS:
		return HL7138_ADC_IBUS;
	case ADC_CHANNEL_VBAT:
		return HL7138_ADC_VBAT;
	case ADC_CHANNEL_IBAT:
		return HL7138_ADC_IBAT;
	case ADC_CHANNEL_TEMP_JC:
		return HL7138_ADC_TDIE;
	case ADC_CHANNEL_VOUT:
		return HL7138_ADC_VOUT;
	default:
		return -ENOTSUPP;
	}
}

static int hl7138_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	int idx = hl7138_to_adc_chan(chan);
	u8 buf[2] = { 0 };
	u32 raw;
	int val = 25;
	int ret;

	if (idx < 0 || idx >= HL7138_ADC_MAX)
		return -ENOTSUPP;

	mutex_lock(&chip->adc_lock);
	if (!chip->force_adc_en) {
		hl7138_set_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
		usleep_range(25000, 30000);
	}

	ret = __hl7138_i2c_read_block(chip, hl7138_adc_reg[idx], 2, buf);
	if (ret < 0) {
		dev_err(chip->dev, "%s %s fail(%d)\n", "__hl7138_get_adc",
			hl7138_adc_name[idx], ret);
		goto out;
	}

	raw = ((u32)buf[0] << 4) | (buf[1] & 0x0f);
	switch (idx) {
	case HL7138_ADC_VBUS:
		val = raw * 4000;
		break;
	case HL7138_ADC_IBUS:
		val = raw * 1100;
		break;
	case HL7138_ADC_VBAT:
	case HL7138_ADC_VOUT:
		val = raw * 1250;
		break;
	case HL7138_ADC_IBAT:
		val = raw * 2200;
		break;
	case HL7138_ADC_VTS:
	case HL7138_ADC_TDIE:
		val = 25;
		break;
	default:
		ret = -ENOTSUPP;
		goto out;
	}

	if (min)
		*min = val;
	if (max && max != min)
		*max = val;
	dev_info(chip->dev, "%s %s %d\n", "__hl7138_get_adc",
		 hl7138_adc_name[idx], val);

out:
	if (!chip->force_adc_en)
		hl7138_clr_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
	mutex_unlock(&chip->adc_lock);
	return ret;
}

static int hl7138_get_adc_accuracy(struct charger_device *chg_dev,
				   enum adc_channel chan, int *min, int *max)
{
	int idx = hl7138_to_adc_chan(chan);

	if (idx < 0 || idx >= HL7138_ADC_MAX)
		return -ENOTSUPP;
	if (min)
		*min = hl7138_adc_accuracy_tbl[idx];
	if (max)
		*max = hl7138_adc_accuracy_tbl[idx];
	return 0;
}

static int hl7138_rfc_reset(struct charger_device *chg_dev)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;

	hl7138_i2c_read8(chip, HL7138_REG_DPDM_CFG0, &val);
	dev_info(chip->dev, "%s DPDM_CFG 0 = %d\n", __func__, val);
	hl7138_i2c_write8(chip, HL7138_REG_DPDM_CFG0, 0x00);
	hl7138_i2c_read8(chip, HL7138_REG_DPDM_CFG1, &val);
	dev_info(chip->dev, "%s DPDM_CFG 1 = %d\n", __func__, val);
	return 0;
}

static int hl7138_set_dp_dm(struct charger_device *chg_dev,
			    enum dpdm_ctrl_status dp, enum dpdm_ctrl_status dm,
			    bool en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = ((u8)dp << 4) | ((u8)dm & 0x0f);

	return hl7138_i2c_write8(chip, HL7138_REG_DPDM_CFG0, val);
}

static int hl7138_get_dp_dm(struct charger_device *chg_dev, bool en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);
	u8 val = 0;

	hl7138_i2c_read8(chip, HL7138_REG_DPDM_CFG0, &val);
	return val;
}

static int hl7138_i2c_trans(struct charger_device *chg_dev, bool en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);

	return hl7138_update_bits(chip, HL7138_REG_CTRL3, BIT(6), en ? BIT(6) : 0);
}

static int hl7138_adc_init(struct charger_device *chg_dev, bool en)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);

	chip->force_adc_en = en;
	return hl7138_update_bits(chip, HL7138_REG_ADC_CTRL0,
				  HL7138_ADC_EN_MASK, en ? HL7138_ADC_EN_MASK : 0);
}

static int hl7138_reset_ucp(struct charger_device *chg_dev)
{
	struct hl7138_chip *chip = charger_dev_get_drvdata(chg_dev);

	return hl7138_clr_bits(chip, HL7138_REG_CTRL1, BIT(0));
}

static const struct charger_ops hl7138_chg_ops = {
	.plug_out = hl7138_plug_out,
	.enable = hl7138_enable_chg,
	.is_enabled = hl7138_is_chg_enabled,
	.set_ibusocp = hl7138_set_ibusocp,
	.set_vbusovp = hl7138_set_vbusovp,
	.set_ibatocp = hl7138_set_ibatocp,
	.set_vbatovp = hl7138_set_vbatovp,
	.set_vbatovp_alarm = hl7138_set_vbatovp_alarm,
	.set_vbusovp_alarm = hl7138_set_vbusovp_alarm,
	.is_vbuslowerr = hl7138_is_vbuslowerr,
	.init_chip = hl7138_init_chip,
	.get_adc = hl7138_get_adc,
	.get_adc_accuracy = hl7138_get_adc_accuracy,
	.soft_reset = hl7138_rfc_reset,
	.set_dp_dm = hl7138_set_dp_dm,
	.get_dp_dm = hl7138_get_dp_dm,
	.i2c_trans = hl7138_i2c_trans,
	.init_adc = hl7138_adc_init,
	.reset_ucp = hl7138_reset_ucp,
};

/* Sysfs debug interface */
static ssize_t hl7138_show_reg(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct hl7138_chip *chip = dev_get_drvdata(dev);
	u8 reg, val = 0;
	int len = 0;

	for (reg = 0x00; reg <= 0x16; reg++) {
		if (hl7138_i2c_read8(chip, reg, &val) == 0)
			len += sprintf(buf + len, "reg[0x%02x]=0x%02x\n", reg, val);
	}
	return len;
}

static ssize_t hl7138_store_reg(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct hl7138_chip *chip = dev_get_drvdata(dev);
	unsigned int reg = 0, val = 0;

	if (sscanf(buf, "%x %x", &reg, &val) == 2)
		hl7138_i2c_write8(chip, (u8)reg, (u8)val);
	return count;
}

static const DEVICE_ATTR(show_reg, 0664, hl7138_show_reg, hl7138_store_reg);

static int hl7138_parse_dt(struct hl7138_chip *chip)
{
	struct device_node *np = chip->dev->of_node;
	struct device_node *child;
	struct hl7138_desc *desc;
	int i;

	desc = devm_kzalloc(chip->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	*desc = hl7138_desc_defval;
	chip->desc = desc;

	if (!np)
		return 0;

	child = of_get_child_by_name(np, hl7138_type_name[chip->type & 0x03]);
	if (!child)
		child = np;

	of_property_read_string(child, "chg_name", &desc->chg_name);

	for (i = 0; i < ARRAY_SIZE(hl7138_dtprops_bool); i++) {
		const struct hl7138_dtprop_bool *p = &hl7138_dtprops_bool[i];

		if (of_property_read_bool(child, p->name))
			*(bool *)((u8 *)desc + p->offset) = true;
	}

	for (i = 0; i < ARRAY_SIZE(hl7138_dtprops_u32); i++) {
		const struct hl7138_dtprop_u32 *p = &hl7138_dtprops_u32[i];
		u32 val = 0;

		if (of_property_read_u32(child, p->name, &val) == 0)
			*(u32 *)((u8 *)desc + p->offset) = val;
	}

	chip->irq_gpio = devm_gpiod_get(chip->dev, "hl7138,intr", GPIOD_IN);
	if (IS_ERR(chip->irq_gpio))
		chip->irq_gpio = NULL;

	return 0;
}

static int hl7138_check_devinfo(struct hl7138_chip *chip)
{
	int ret, type_reg;

	ret = i2c_smbus_read_byte_data(chip->client, HL7138_REG_DEVICE_ID);
	dev_info(chip->dev, "%s ret=0x%x\n", __func__, ret);
	if (ret < 0)
		return ret;

	chip->chip_id = ret & 0x0f;
	chip->rev = (ret >> 4) & 0x0f;
	if (chip->chip_id != HL7138_DEVID && chip->chip_id != HL7139_DEVID)
		return -ENODEV;

	type_reg = i2c_smbus_read_byte_data(chip->client, HL7138_REG_STATUS_B);
	chip->type = (type_reg >= 0) ? (type_reg & 0x03) : 3;
	dev_info(chip->dev, "%s rev(0x%02X), type(%s)\n", __func__,
		 chip->rev, hl7138_type_name[chip->type & 0x03]);
	return 0;
}

static int hl7138_init_irq(struct hl7138_chip *chip)
{
	char *irq_name;
	u8 dummy;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(hl7138_reg_sf); i++) {
		ret = hl7138_i2c_read8(chip, hl7138_reg_sf[i], &dummy);
		if (ret < 0) {
			dev_err(chip->dev, "%s clr all irq fail(%d)\n", __func__, ret);
			return ret;
		}
	}

	if (!chip->irq_gpio)
		return 0;

	chip->irq = gpiod_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		dev_err(chip->dev, "%s irq mapping fail(%d)\n", __func__, chip->irq);
		return chip->irq;
	}
	dev_info(chip->dev, "%s irq = %d\n", __func__, chip->irq);

	irq_name = devm_kzalloc(chip->dev, strlen(chip->desc->chg_name) + 8, GFP_KERNEL);
	if (!irq_name)
		return -ENOMEM;
	snprintf(irq_name, strlen(chip->desc->chg_name) + 8, "%s_irq",
		 chip->desc->chg_name);

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					hl7138_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					irq_name, chip);
	if (ret < 0)
		dev_err(chip->dev, "%s request thread irq fail(%d)\n", __func__, ret);

	return ret;
}

static int hl7138_i2c_probe(struct i2c_client *client)
{
	struct hl7138_chip *chip;
	int ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->client = client;
	i2c_set_clientdata(client, chip);

	mutex_init(&chip->io_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->stat_lock);
	mutex_init(&chip->notify_lock);
	init_waitqueue_head(&chip->wq);
	INIT_WORK(&chip->state_work, hl7138_state_update_handler);

	ret = hl7138_check_devinfo(chip);
	if (ret < 0)
		return ret;

	ret = hl7138_parse_dt(chip);
	if (ret < 0)
		return ret;

	ret = __hl7138_init_chip(chip);
	if (ret < 0)
		return ret;

	chip->chg_prop.alias_name = chip->desc->chg_name;
	chip->chg_dev = charger_device_register(chip->desc->chg_name, chip->dev,
						chip, &hl7138_chg_ops,
						&chip->chg_prop);
	if (IS_ERR_OR_NULL(chip->chg_dev))
		return -EINVAL;

	chip->notify_task = kthread_run(hl7138_notify_task_threadfn, chip,
					"hl7138_notify");
	if (IS_ERR(chip->notify_task)) {
		ret = PTR_ERR(chip->notify_task);
		charger_device_unregister(chip->chg_dev);
		return ret;
	}

	ret = hl7138_init_irq(chip);
	if (ret < 0)
		dev_err(chip->dev, "%s init irq fail(%d)\n", __func__, ret);

	device_create_file(chip->dev, &dev_attr_show_reg);
	device_init_wakeup(chip->dev, true);

	if (chip->chip_id == HL7139_DEVID)
		hl7138_rfc_reset(chip->chg_dev);

	dev_info(chip->dev, "%s successfully\n", __func__);
	return 0;
}

static void hl7138_i2c_remove(struct i2c_client *client)
{
	struct hl7138_chip *chip = i2c_get_clientdata(client);

	if (!chip)
		return;
	if (chip->irq > 0)
		disable_irq(chip->irq);
	if (chip->notify_task && !IS_ERR(chip->notify_task))
		kthread_stop(chip->notify_task);
	if (chip->chg_dev)
		charger_device_unregister(chip->chg_dev);
}

static void hl7138_i2c_shutdown(struct i2c_client *client)
{
	struct hl7138_chip *chip = i2c_get_clientdata(client);

	if (!chip)
		return;
	hl7138_clr_bits(chip, HL7138_REG_CTRL0, HL7138_CHG_EN_MASK);
	hl7138_clr_bits(chip, HL7138_REG_ADC_CTRL0, HL7138_ADC_EN_MASK);
}

static int hl7138_i2c_suspend(struct device *dev)
{
	struct hl7138_chip *chip = dev_get_drvdata(dev);

	if (chip && chip->irq > 0 && device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	return 0;
}

static int hl7138_i2c_resume(struct device *dev)
{
	struct hl7138_chip *chip = dev_get_drvdata(dev);

	if (chip && chip->irq > 0 && device_may_wakeup(dev))
		disable_irq_wake(chip->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(hl7138_pm_ops, hl7138_i2c_suspend, hl7138_i2c_resume);

static const struct of_device_id hl7138_of_id[] = {
	{ .compatible = "richtek,hl7138" },
	{ .compatible = "richtek,hl7139" },
	{},
};
MODULE_DEVICE_TABLE(of, hl7138_of_id);

static const struct i2c_device_id hl7138_i2c_id[] = {
	{ "hl7138", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, hl7138_i2c_id);

static struct i2c_driver hl7138_i2c_driver = {
	.driver = {
		.name = "hl7138",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hl7138_of_id),
		.pm = &hl7138_pm_ops,
	},
	.probe = hl7138_i2c_probe,
	.remove = hl7138_i2c_remove,
	.shutdown = hl7138_i2c_shutdown,
	.id_table = hl7138_i2c_id,
};

module_i2c_driver(hl7138_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ShuFan Lee<shufan_lee@richtek.com>");
MODULE_DESCRIPTION("HL7138 Charger Driver");
MODULE_VERSION("1.0.7_MTK");
