/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include "aw87xxx_device.h"
#include "aw87xxx_pid_reg.h"
#include "aw87xxx_log.h"

static unsigned char aw87xxx_pid_9b_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_9b_product[] = {
	"aw87319",
};
static const unsigned char aw87xxx_pid_9b_reg_access[AW_PID_9B_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
};

static unsigned char aw87xxx_pid_39_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_39_product[] = {
	"aw87329",
	"aw87339",
	"aw87349",
};
static const unsigned char aw87xxx_pid_39_reg_access[AW_PID_39_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
	[0x0a] = REG_RDWR_ACCESS,
	[0x62] = REG_RD_ACCESS,
	[0x63] = REG_RD_ACCESS,
	[0x64] = REG_RD_ACCESS,
};

static unsigned char aw87xxx_pid_59_5x9_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_59_5x9_product[] = {
	"aw87509",
	"aw87519",
	"aw87529",
	"aw87539",
};
static const unsigned char aw87xxx_pid_59_5x9_reg_access[AW_PID_59_5X9_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
	[0x0a] = REG_RDWR_ACCESS,
	[0x58] = REG_RD_ACCESS,
	[0x59] = REG_RD_ACCESS,
	[0x60] = REG_RDWR_ACCESS,
	[0x61] = REG_RDWR_ACCESS,
	[0x62] = REG_RDWR_ACCESS,
	[0x63] = REG_RDWR_ACCESS,
	[0x64] = REG_RDWR_ACCESS,
	[0x65] = REG_RDWR_ACCESS,
	[0x66] = REG_RDWR_ACCESS,
	[0x67] = REG_RDWR_ACCESS,
	[0x68] = REG_RDWR_ACCESS,
	[0x69] = REG_RDWR_ACCESS,
};

static unsigned char aw87xxx_pid_59_3x9_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_59_3x9_product[] = {
	"aw87359",
	"aw87389",
};
static const unsigned char aw87xxx_pid_59_3x9_reg_access[AW_PID_59_3X9_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
	[0x0a] = REG_RDWR_ACCESS,
	[0x59] = REG_RD_ACCESS,
	[0x60] = REG_RD_ACCESS,
	[0x61] = REG_RDWR_ACCESS,
	[0x62] = REG_RDWR_ACCESS,
	[0x63] = REG_RDWR_ACCESS,
	[0x64] = REG_RDWR_ACCESS,
	[0x65] = REG_RDWR_ACCESS,
	[0x66] = REG_RDWR_ACCESS,
	[0x67] = REG_RDWR_ACCESS,
	[0x68] = REG_RDWR_ACCESS,
	[0x69] = REG_RDWR_ACCESS,
	[0x70] = REG_RDWR_ACCESS,
};

static unsigned char aw87xxx_pid_5a_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_5a_product[] = {
	"aw87549",
	"aw87559",
	"aw87569",
	"aw87579",
	"aw81509",
};
static const unsigned char aw87xxx_pid_5a_reg_access[AW_PID_5A_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
	[0x0a] = REG_RDWR_ACCESS,
	[0x0b] = REG_RDWR_ACCESS,
	[0x0c] = REG_RDWR_ACCESS,
	[0x0d] = REG_RDWR_ACCESS,
	[0x0e] = REG_RDWR_ACCESS,
	[0x0f] = REG_RDWR_ACCESS,
	[0x10] = REG_RDWR_ACCESS,
	[0x57] = REG_RD_ACCESS,
	[0x58] = REG_RD_ACCESS,
	[0x59] = REG_RD_ACCESS,
	[0x60] = REG_RDWR_ACCESS,
	[0x61] = REG_RDWR_ACCESS,
	[0x62] = REG_RDWR_ACCESS,
	[0x63] = REG_RDWR_ACCESS,
	[0x64] = REG_RDWR_ACCESS,
	[0x65] = REG_RDWR_ACCESS,
	[0x66] = REG_RDWR_ACCESS,
	[0x67] = REG_RDWR_ACCESS,
	[0x68] = REG_RDWR_ACCESS,
	[0x69] = REG_RDWR_ACCESS,
	[0x70] = REG_RDWR_ACCESS,
	[0x71] = REG_RDWR_ACCESS,
	[0x72] = REG_RDWR_ACCESS,
	[0x73] = REG_RDWR_ACCESS,
	[0x74] = REG_RDWR_ACCESS,
	[0x75] = REG_RDWR_ACCESS,
	[0x76] = REG_RDWR_ACCESS,
	[0x77] = REG_RDWR_ACCESS,
};

static unsigned char aw87xxx_pid_76_softrst_access[2] = {
	AW_PID_SOFTRST_REG, AW_PID_SOFTRST_VAL
};
static const char *g_aw_pid_76_product[] = {
	"aw87390",
	"aw87320",
	"aw87401",
};
static const unsigned char aw87xxx_pid_76_reg_access[AW_PID_76_REG_MAX] = {
	[0x00] = REG_RD_ACCESS,
	[0x01] = REG_RDWR_ACCESS,
	[0x02] = REG_RDWR_ACCESS,
	[0x03] = REG_RDWR_ACCESS,
	[0x04] = REG_RDWR_ACCESS,
	[0x05] = REG_RDWR_ACCESS,
	[0x06] = REG_RDWR_ACCESS,
	[0x07] = REG_RDWR_ACCESS,
	[0x08] = REG_RDWR_ACCESS,
	[0x09] = REG_RDWR_ACCESS,
	[0x0a] = REG_RDWR_ACCESS,
	[0x59] = REG_RD_ACCESS,
	[0x60] = REG_RD_ACCESS,
	[0x61] = REG_RDWR_ACCESS,
	[0x62] = REG_RDWR_ACCESS,
	[0x63] = REG_RDWR_ACCESS,
	[0x64] = REG_RDWR_ACCESS,
	[0x65] = REG_RDWR_ACCESS,
	[0x66] = REG_RDWR_ACCESS,
	[0x67] = REG_RDWR_ACCESS,
	[0x68] = REG_RDWR_ACCESS,
	[0x69] = REG_RDWR_ACCESS,
	[0x70] = REG_RDWR_ACCESS,
	[0x71] = REG_RDWR_ACCESS,
	[0x72] = REG_RDWR_ACCESS,
	[0x73] = REG_RDWR_ACCESS,
	[0x74] = REG_RDWR_ACCESS,
	[0x75] = REG_RDWR_ACCESS,
	[0x76] = REG_RDWR_ACCESS,
	[0x77] = REG_RDWR_ACCESS,
	[0x78] = REG_RDWR_ACCESS,
};

int aw_dev_i2c_write_byte(struct aw_device *aw_dev, u8 reg_addr, u8 reg_data)
{
	int ret = -1;
	u8 cnt = 0;

	while (cnt < AW_DEV_I2C_RETRY_TIMES) {
		ret = i2c_smbus_write_byte_data(aw_dev->i2c, reg_addr, reg_data);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "i2c_write cnt=%d error=%d",
				    cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_DEV_I2C_RETRY_DELAY_MS);
	}

	return ret;
}

int aw_dev_i2c_read_byte(struct aw_device *aw_dev, u8 reg_addr, u8 *reg_data)
{
	int ret = -1;
	u8 cnt = 0;

	while (cnt < AW_DEV_I2C_RETRY_TIMES) {
		ret = i2c_smbus_read_byte_data(aw_dev->i2c, reg_addr);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev, "i2c_read cnt=%d error=%d",
				    cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_DEV_I2C_RETRY_DELAY_MS);
	}

	return ret;
}

int aw_dev_i2c_read_msg(struct aw_device *aw_dev, u8 reg_addr, u8 *data_buf,
			u32 data_len)
{
	int ret = -1;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw_dev->i2c_addr,
			.flags = 0,
			.len = sizeof(u8),
			.buf = &reg_addr,
		},
		[1] = {
			.addr = aw_dev->i2c_addr,
			.flags = I2C_M_RD,
			.len = data_len,
			.buf = data_buf,
		},
	};

	ret = i2c_transfer(aw_dev->i2c->adapter, msg, AW_DEV_I2C_READ_MSG_NUM);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "transfer failed");
		return ret;
	} else if (ret != AW_DEV_I2C_READ_MSG_NUM) {
		AW_DEV_LOGE(aw_dev->dev, "transfer failed(size error)");
		return -ENXIO;
	}

	return 0;
}

int aw_dev_i2c_write_bits(struct aw_device *aw_dev, u8 reg_addr, u16 mask,
			  u8 reg_data)
{
	int ret = -1;
	u8 reg_val = 0;

	ret = aw_dev_i2c_read_byte(aw_dev, reg_addr, &reg_val);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "i2c read error, ret=%d", ret);
		return ret;
	}

	reg_val &= (u8)mask;
	reg_val |= reg_data;

	ret = aw_dev_i2c_write_byte(aw_dev, reg_addr, reg_val);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "i2c write error, ret=%d", ret);
		return ret;
	}

	return 0;
}

void aw_dev_hw_pwr_ctrl(struct aw_device *aw_dev, bool enable)
{
	if (aw_dev->hwen_status == AW_DEV_HWEN_INVALID) {
		AW_DEV_LOGI(aw_dev->dev,
			    "product not have reset-pin,hardware pwd control invalid");
		return;
	}

	if (enable) {
		if (aw_dev->hwen_status == AW_DEV_HWEN_ON) {
			AW_DEV_LOGI(aw_dev->dev, "hwen is already on");
			return;
		}
		if (gpio_is_valid(aw_dev->rst_gpio)) {
			gpio_set_value_cansleep(aw_dev->rst_gpio, 0);
			mdelay(AW_DEV_HWEN_DELAY_MS);
			gpio_set_value_cansleep(aw_dev->rst_gpio, 1);
			mdelay(AW_DEV_HWEN_DELAY_MS);
			aw_dev->hwen_status = AW_DEV_HWEN_ON;
			AW_DEV_LOGI(aw_dev->dev, "hw power on");
		} else {
			AW_DEV_LOGI(aw_dev->dev, "hw already power on");
		}
	} else {
		if (aw_dev->hwen_status == AW_DEV_HWEN_OFF) {
			AW_DEV_LOGI(aw_dev->dev, "hwen is already off");
			return;
		}
		if (gpio_is_valid(aw_dev->rst_gpio)) {
			gpio_set_value_cansleep(aw_dev->rst_gpio, 0);
			mdelay(AW_DEV_HWEN_DELAY_MS);
			aw_dev->hwen_status = AW_DEV_HWEN_OFF;
			AW_DEV_LOGI(aw_dev->dev, "hw power off");
		} else {
			AW_DEV_LOGI(aw_dev->dev, "hw already power off");
		}
	}
}

int aw_dev_mute_ctrl(struct aw_device *aw_dev, bool enable)
{
	int ret = -1;

	if (enable) {
		ret = aw_dev_i2c_write_bits(aw_dev, aw_dev->mute_desc.addr,
					    aw_dev->mute_desc.mask,
					    aw_dev->mute_desc.enable);
		if (ret < 0)
			return ret;
		AW_DEV_LOGI(aw_dev->dev, "set mute down");
	} else {
		ret = aw_dev_i2c_write_bits(aw_dev, aw_dev->mute_desc.addr,
					    aw_dev->mute_desc.mask,
					    aw_dev->mute_desc.disable);
		if (ret < 0)
			return ret;
		AW_DEV_LOGI(aw_dev->dev, "close mute down");
	}

	return 0;
}

void aw_dev_soft_reset(struct aw_device *aw_dev)
{
	int i = 0;
	int ret = -1;

	if (aw_dev->hwen_status == AW_DEV_HWEN_OFF) {
		AW_DEV_LOGE(aw_dev->dev, "hw is off,can not softrst");
		return;
	}

	if (aw_dev->soft_rst_enable == AW_DEV_SOFT_RST_DISABLE) {
		AW_DEV_LOGI(aw_dev->dev, "softrst is disable");
		return;
	}

	if (!aw_dev->soft_rst_access || aw_dev->soft_rst_len == 0) {
		AW_DEV_LOGE(aw_dev->dev, "softrst_info not init");
		return;
	}

	if (aw_dev->soft_rst_len % 2) {
		AW_DEV_LOGE(aw_dev->dev,
			    "softrst data_len[%d] is odd number,data not available",
			    aw_dev->soft_rst_len);
		return;
	}

	for (i = 0; i < aw_dev->soft_rst_len; i += 2) {
		ret = aw_dev_i2c_write_byte(aw_dev,
					    aw_dev->soft_rst_access[i],
					    aw_dev->soft_rst_access[i + 1]);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev,
				    "write failed,ret = %d,cnt=%d", ret, i);
			return;
		}
	}

	AW_DEV_LOGI(aw_dev->dev, "softrst succeed");
}

static int aw_dev_reg_update(struct aw_device *aw_dev,
			     struct aw_data_container *data_container)
{
	int i = 0;
	int ret = -1;

	if (!data_container)
		return -EINVAL;

	if (aw_dev->hwen_status == AW_DEV_HWEN_OFF) {
		AW_DEV_LOGE(aw_dev->dev, "dev is pwr_off,can not update reg");
		return -EINVAL;
	}

	for (i = 0; i < data_container->len; i += 2) {
		AW_DEV_LOGI(aw_dev->dev, "reg=0x%02x, val = 0x%02x",
			    data_container->data[i],
			    data_container->data[i + 1]);
		ret = aw_dev_i2c_write_byte(aw_dev,
					    data_container->data[i],
					    data_container->data[i + 1]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int aw_dev_default_pwr_off(struct aw_device *aw_dev,
			   struct aw_data_container *data_container)
{
	int ret = 0;

	if (aw_dev->hwen_status == AW_DEV_HWEN_OFF) {
		AW_DEV_LOGE(aw_dev->dev, "hwen is already off");
		return 0;
	}

	if (aw_dev->soft_off_enable && data_container) {
		ret = aw_dev_reg_update(aw_dev, data_container);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev,
				    "update profile[Off] fw config failed");
			goto power_off;
		}
	}

power_off:
	aw_dev_hw_pwr_ctrl(aw_dev, false);
	return ret;
}

int aw_dev_default_pwr_on(struct aw_device *aw_dev,
			  struct aw_data_container *data_container)
{
	int ret = 0;

	aw_dev_hw_pwr_ctrl(aw_dev, true);

	ret = aw_dev_reg_update(aw_dev, data_container);
	if (ret < 0)
		return ret;

	return 0;
}

int aw_dev_esd_reg_status_check(struct aw_device *aw_dev)
{
	int ret = 0;
	u8 reg_val = 0;

	if (!aw_dev->esd_desc.first_update_reg_addr) {
		AW_DEV_LOGE(aw_dev->dev,
			    "esd check info if not init,please check");
		return -EINVAL;
	}

	ret = aw_dev_i2c_read_byte(aw_dev,
				   aw_dev->esd_desc.first_update_reg_addr,
				   &reg_val);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "read reg 0x%02x failed",
			    aw_dev->esd_desc.first_update_reg_addr);
		return ret;
	}

	if (reg_val == aw_dev->esd_desc.first_update_reg_val) {
		AW_DEV_LOGE(aw_dev->dev, "reg status check failed");
		return -EINVAL;
	}

	return 0;
}

int aw_dev_check_reg_is_rec_mode(struct aw_device *aw_dev)
{
	int ret = 0;
	u8 reg_val = 0;

	if (!aw_dev->rec_desc.addr) {
		AW_DEV_LOGE(aw_dev->dev,
			    "rec check info if not init,please check");
		return -EINVAL;
	}

	ret = aw_dev_i2c_read_byte(aw_dev, aw_dev->rec_desc.addr, &reg_val);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "read reg 0x%02x failed",
			    aw_dev->rec_desc.addr);
		return ret;
	}

	if (aw_dev->rec_desc.enable) {
		if (reg_val & (~aw_dev->rec_desc.mask)) {
			AW_DEV_LOGI(aw_dev->dev, "reg status is receiver mode");
			aw_dev->is_rec_mode = 1;
		} else {
			aw_dev->is_rec_mode = 0;
		}
	} else {
		if (!(reg_val & (~aw_dev->rec_desc.mask))) {
			AW_DEV_LOGI(aw_dev->dev, "reg status is receiver mode");
			aw_dev->is_rec_mode = 1;
		} else {
			aw_dev->is_rec_mode = 0;
		}
	}

	return 0;
}

static int aw_dev_pid_9b_reg_update(struct aw_device *aw_dev,
				    struct aw_data_container *data_container)
{
	int i = 0;
	int ret = -1;
	u8 reg_val = 0;

	if (!data_container)
		return -EINVAL;

	if (aw_dev->hwen_status == AW_DEV_HWEN_OFF) {
		AW_DEV_LOGE(aw_dev->dev, "dev is pwr_off,can not update reg");
		return -EINVAL;
	}

	if (data_container->len != 10) {
		AW_DEV_LOGE(aw_dev->dev,
			    "reg_config count of bin is error,can not update reg");
		return -EINVAL;
	}

	ret = aw_dev_i2c_write_byte(aw_dev, AW_PID_9B_ENCRYPTION_REG,
				    AW_PID_9B_ENCRYPTION_BOOST);
	if (ret < 0)
		return ret;

	for (i = 1; i < 10; i++) {
		AW_DEV_LOGI(aw_dev->dev, "reg=0x%02x, val = 0x%02x",
			    i, data_container->data[i]);
		reg_val = data_container->data[i];
		if (i == 1)
			reg_val = (reg_val & (u8)aw_dev->mute_desc.mask) |
				  aw_dev->mute_desc.enable;
		ret = aw_dev_i2c_write_byte(aw_dev, i, reg_val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int aw_dev_pid_9b_pwr_on(struct aw_device *aw_dev,
			 struct aw_data_container *data_container)
{
	int ret = 0;

	aw_dev_hw_pwr_ctrl(aw_dev, true);

	ret = aw_dev_mute_ctrl(aw_dev, true);
	if (ret < 0)
		return ret;

	ret = aw_dev_pid_9b_reg_update(aw_dev, data_container);
	if (ret < 0)
		return ret;

	aw_dev_mute_ctrl(aw_dev, false);

	return 0;
}

static int aw_dev_get_chipid(struct aw_device *aw_dev)
{
	int ret = -1;
	u32 cnt = 0;
	u8 reg_val = 0;

	for (cnt = 0; cnt < AW_DEV_CHIPID_RETRY_TIMES; cnt++) {
		ret = aw_dev_i2c_read_byte(aw_dev, AW87XXX_REG_CHIPID, &reg_val);
		if (ret < 0) {
			AW_DEV_LOGE(aw_dev->dev,
				    "read chip id error,ret=%d", ret);
			continue;
		}
		AW_DEV_LOGI(aw_dev->dev, "read chipid[0x%x] succeed", reg_val);
		aw_dev->chipid = reg_val;
		return 0;
	}

	return -EINVAL;
}

static void aw_dev_pid_9b_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_9B_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_9b_product);
	aw_dev->product_tab = g_aw_pid_9b_product;
	aw_dev->reg_access = aw87xxx_pid_9b_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_DISABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_9b_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_9b_softrst_access;

	aw_dev->mute_desc.addr = AW_PID_9B_SYSCTRL_REG;
	aw_dev->mute_desc.enable = AW_PID_9B_SPK_MODE_DISABLE;
	aw_dev->mute_desc.disable = AW_PID_9B_SPK_MODE_ENABLE;
	aw_dev->mute_desc.mask = AW_PID_9B_SPK_MODE_MASK;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_9B_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_9B_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_9B_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_9B_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_9B_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_9B_REC_MASK;

	aw_dev->ops.pwr_on_func = aw_dev_pid_9b_pwr_on;
}

static int aw_dev_pid_9a_init(struct aw_device *aw_dev)
{
	int ret = 0;

	ret = aw_dev_i2c_write_byte(aw_dev, AW_PID_9B_ENCRYPTION_REG,
				    AW_PID_9B_ENCRYPTION_BOOST);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "write 0x64=0x2C error");
		return -EINVAL;
	}

	ret = aw_dev_get_chipid(aw_dev);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "read chipid is failed,ret=%d", ret);
		return -EINVAL;
	}

	if (aw_dev->chipid == AW_DEV_CHIPID_9B) {
		AW_DEV_LOGI(aw_dev->dev, "product is pid_9B class");
		aw_dev_pid_9b_init(aw_dev);
		return 0;
	}

	AW_DEV_LOGE(aw_dev->dev, "product is not pid_9B class, not support");
	return -EINVAL;
}

static void aw_dev_pid_39_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_39_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_39_product);
	aw_dev->product_tab = g_aw_pid_39_product;
	aw_dev->reg_access = aw87xxx_pid_39_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_ENABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_39_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_39_softrst_access;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_39_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_39_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_39_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_39_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_39_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_39_REC_MASK;
}

static void aw_dev_pid_59_5x9_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_59_5X9_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_59_5x9_product);
	aw_dev->product_tab = g_aw_pid_59_5x9_product;
	aw_dev->reg_access = aw87xxx_pid_59_5x9_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_ENABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_59_5x9_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_59_5x9_softrst_access;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_59_5X9_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_59_5X9_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_59_5X9_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_59_5X9_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_59_5X9_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_59_5X9_REC_MASK;
}

static void aw_dev_pid_59_3x9_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_59_3X9_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_59_3x9_product);
	aw_dev->product_tab = g_aw_pid_59_3x9_product;
	aw_dev->reg_access = aw87xxx_pid_59_3x9_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_ENABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_59_3x9_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_59_3x9_softrst_access;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_59_3X9_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_59_3X9_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_59_3X9_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_59_3X9_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_59_3X9_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_59_3X9_REC_MASK;
}

static void aw_dev_pid_5a_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_5A_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_5a_product);
	aw_dev->product_tab = g_aw_pid_5a_product;
	aw_dev->reg_access = aw87xxx_pid_5a_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_ENABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_5a_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_5a_softrst_access;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_5A_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_5A_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_5A_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_5A_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_5A_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_5A_REC_MASK;
}

static void aw_dev_pid_76_init(struct aw_device *aw_dev)
{
	aw_dev->reg_max_addr = AW_PID_76_REG_MAX;
	aw_dev->product_cnt = ARRAY_SIZE(g_aw_pid_76_product);
	aw_dev->product_tab = g_aw_pid_76_product;
	aw_dev->reg_access = aw87xxx_pid_76_reg_access;

	aw_dev->soft_rst_enable = AW_DEV_SOFT_RST_ENABLE;
	aw_dev->soft_off_enable = AW_DEV_SOFT_OFF_ENABLE;
	aw_dev->soft_rst_len = sizeof(aw87xxx_pid_76_softrst_access);
	aw_dev->soft_rst_access = aw87xxx_pid_76_softrst_access;

	aw_dev->esd_desc.first_update_reg_addr = AW_PID_76_ESD_REG;
	aw_dev->esd_desc.first_update_reg_val = AW_PID_76_ESD_CHECK_VAL;

	aw_dev->rec_desc.addr = AW_PID_76_REC_REG;
	aw_dev->rec_desc.enable = AW_PID_76_REC_ENABLE;
	aw_dev->rec_desc.disable = AW_PID_76_REC_DISABLE;
	aw_dev->rec_desc.mask = AW_PID_76_REC_MASK;
}

static int aw_dev_chip_init(struct aw_device *aw_dev)
{
	int ret = 0;

	switch (aw_dev->chipid) {
	case AW_DEV_CHIPID_9A:
		ret = aw_dev_pid_9a_init(aw_dev);
		break;
	case AW_DEV_CHIPID_9B:
		aw_dev_pid_9b_init(aw_dev);
		AW_DEV_LOGI(aw_dev->dev, "product is pid_9B class");
		break;
	case AW_DEV_CHIPID_39:
		aw_dev_pid_39_init(aw_dev);
		AW_DEV_LOGI(aw_dev->dev, "product is pid_39 class");
		break;
	case AW_DEV_CHIPID_59:
		if (gpio_is_valid(aw_dev->rst_gpio)) {
			aw_dev_pid_59_5x9_init(aw_dev);
			AW_DEV_LOGI(aw_dev->dev, "product is pid_59_5x9 class");
		} else {
			aw_dev_pid_59_3x9_init(aw_dev);
			AW_DEV_LOGI(aw_dev->dev, "product is pid_59_3x9 class");
		}
		break;
	case AW_DEV_CHIPID_5A:
		aw_dev_pid_5a_init(aw_dev);
		AW_DEV_LOGI(aw_dev->dev, "product is pid_5A class");
		break;
	case AW_DEV_CHIPID_76:
		aw_dev_pid_76_init(aw_dev);
		AW_DEV_LOGI(aw_dev->dev, "product is pid_76 class");
		break;
	default:
		AW_DEV_LOGE(aw_dev->dev, "unsupported device revision [0x%x]",
			    aw_dev->chipid);
		return -EINVAL;
	}

	return ret;
}

int aw_dev_init(struct aw_device *aw_dev)
{
	int ret = -1;

	ret = aw_dev_get_chipid(aw_dev);
	if (ret < 0) {
		AW_DEV_LOGE(aw_dev->dev, "read chipid is failed,ret=%d", ret);
		return ret;
	}

	ret = aw_dev_chip_init(aw_dev);
	return ret;
}
