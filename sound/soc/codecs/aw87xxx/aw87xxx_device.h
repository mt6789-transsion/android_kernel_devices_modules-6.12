/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_DEVICE_H__
#define __AW87XXX_DEVICE_H__

#include <linux/i2c.h>
#include <linux/types.h>

#define AW_DEV_I2C_READ_MSG_NUM		2
#define AW_DEV_I2C_RETRY_TIMES		5
#define AW_DEV_I2C_RETRY_DELAY_MS	2
#define AW_DEV_CHIPID_RETRY_TIMES	5
#define AW_DEV_HWEN_DELAY_MS		2

#define AW_DEV_SOFT_RST_ENABLE		1
#define AW_DEV_SOFT_RST_DISABLE		0
#define AW_DEV_SOFT_OFF_ENABLE		1
#define AW_DEV_SOFT_OFF_DISABLE		0

#define AW_DEV_HWEN_OFF			0
#define AW_DEV_HWEN_ON			1
#define AW_DEV_HWEN_INVALID		2

#define AW_DEV_MUTE_ENABLE		1
#define AW_DEV_MUTE_DISABLE		0

#define AW_DEV_REG_INVALID_MASK		0xFF

/*
 * Exact struct tag names `struct aw_device` and `struct aw_data_container`
 * are required so Clang CFI type hash for pwr_on_func / pwr_off_func matches stock.
 */
struct aw_data_container {
	u32 len;
	u8 *data;
};

struct aw_mute_desc {
	u8 addr;
	u8 enable;
	u8 disable;
	u16 mask;
};

struct aw_esd_check_desc {
	u8 first_update_reg_addr;
	u8 first_update_reg_val;
};

struct aw_rec_mode_desc {
	u8 addr;
	u8 enable;
	u8 disable;
	u8 mask;
};

struct aw_device;

struct aw_device_ops {
	int (*pwr_on_func)(struct aw_device *aw_dev,
			   struct aw_data_container *data_container);
	int (*pwr_off_func)(struct aw_device *aw_dev,
			    struct aw_data_container *data_container);
};

struct aw_device {
	u8 i2c_addr;
	u8 chipid;
	u8 soft_rst_enable;
	u8 soft_off_enable;
	int is_rec_mode;
	int hwen_status;
	int i2c_bus;
	int rst_gpio;
	int reg_max_addr;
	int product_cnt;
	const char **product_tab;
	const unsigned char *reg_access;
	struct device *dev;
	struct i2c_client *i2c;
	struct aw_mute_desc mute_desc;
	int soft_rst_len;
	unsigned char *soft_rst_access;
	struct aw_esd_check_desc esd_desc;
	struct aw_rec_mode_desc rec_desc;
	struct aw_device_ops ops;
};

int aw_dev_i2c_write_byte(struct aw_device *aw_dev, u8 reg_addr, u8 reg_data);
int aw_dev_i2c_read_byte(struct aw_device *aw_dev, u8 reg_addr, u8 *reg_data);
int aw_dev_i2c_read_msg(struct aw_device *aw_dev, u8 reg_addr, u8 *data_buf,
			u32 data_len);
int aw_dev_i2c_write_bits(struct aw_device *aw_dev, u8 reg_addr, u16 mask,
			  u8 reg_data);
void aw_dev_hw_pwr_ctrl(struct aw_device *aw_dev, bool enable);
int aw_dev_mute_ctrl(struct aw_device *aw_dev, bool enable);
void aw_dev_soft_reset(struct aw_device *aw_dev);
int aw_dev_default_pwr_off(struct aw_device *aw_dev,
			   struct aw_data_container *data_container);
int aw_dev_default_pwr_on(struct aw_device *aw_dev,
			  struct aw_data_container *data_container);
int aw_dev_pid_9b_pwr_on(struct aw_device *aw_dev,
			 struct aw_data_container *data_container);
int aw_dev_esd_reg_status_check(struct aw_device *aw_dev);
int aw_dev_check_reg_is_rec_mode(struct aw_device *aw_dev);
int aw_dev_init(struct aw_device *aw_dev);

#endif /* __AW87XXX_DEVICE_H__ */
