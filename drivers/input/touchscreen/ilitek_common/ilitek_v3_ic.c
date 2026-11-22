// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI IC control, ICE mode, MSPI, DDI, and protocol info layer
 */
#include "ilitek_v3.h"

#define PROTOCL_VER_NUM		8

static struct ilitek_ic_info chip;

struct ilitek_ic_func_ctrl func_ctrl[] = {
	/* { name, { cmd... }, len, def_cmd, rec_state, rec_cmd } */
	{ "sense",           { 0x01, 0x01, 0x00 }, 3, 0, 2, 0xFF },
	{ "sleep",           { 0x01, 0x02, 0x00 }, 3, 0, 2, 0xFF },
	{ "glove",           { 0x01, 0x06, 0x00 }, 3, 0, 0, 0xFF },
	{ "stylus",          { 0x01, 0x07, 0x00 }, 3, 0, 0, 0xFF },
	{ "lpwg",            { 0x01, 0x0A, 0x00 }, 3, 0, 2, 0xFF },
	{ "proximity",       { 0x01, 0x10, 0x00 }, 3, 0, 0, 0xFF },
	{ "plug",            { 0x01, 0x11, 0x00 }, 3, 1, 0, 0xFF },
	{ "edge_palm",       { 0x01, 0x12, 0x00 }, 3, 1, 0, 0xFF },
	{ "lock_point",      { 0x01, 0x13, 0x00 }, 3, 0, 0, 0xFF },
	{ "active",          { 0x01, 0x14, 0x00 }, 3, 0, 2, 0xFF },
	{ "idle",            { 0x01, 0x19, 0x00 }, 3, 1, 0, 0xFF },
	{ "gesture_demo_en", { 0x01, 0x16, 0x00 }, 3, 0, 0, 0xFF },
	{ "tp_recore",       { 0x01, 0x18, 0x00 }, 3, 0, 2, 0xFF },
	{ "knock_en",        { 0x01, 0x0A, 0x08, 0x03, 0x00, 0x00 }, 6, 0xFF, 0, 0xFF },
	{ "int_trigger",     { 0x01, 0x1B, 0x00 }, 3, 0, 2, 0xFF },
	{ "ear_phone",       { 0x01, 0x17, 0x00 }, 3, 0, 0, 0xFF },
	{ "knuckle",         { 0x01, 0x0F, 0x00 }, 3, 0, 0, 0xFF },
	{ "game_mode",       { 0x01, 0x13, 0x00 }, 3, 0, 0, 0xFF },
};

static struct ilitek_protocol_info protocol_info[PROTOCL_VER_NUM] = {
	[0] = { PROTOCOL_VER_500, 4, 4, 14, 30, 5, 5, 2, 8,  3,  8 },
	[1] = { PROTOCOL_VER_510, 4, 3, 14, 30, 5, 5, 3, 8,  3,  8 },
	[2] = { PROTOCOL_VER_520, 4, 4, 14, 30, 5, 5, 3, 8,  3,  8 },
	[3] = { PROTOCOL_VER_530, 9, 4, 14, 30, 5, 5, 3, 8,  3,  8 },
	[4] = { PROTOCOL_VER_540, 9, 4, 14, 30, 5, 5, 3, 8, 15,  8 },
	[5] = { PROTOCOL_VER_550, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14 },
	[6] = { PROTOCOL_VER_560, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14 },
	[7] = { PROTOCOL_VER_570, 9, 4, 14, 30, 12, 5, 3, 8, 15, 14 },
};

void ili_dump_data(void *data, int type, int len, int row_len, const char *name)
{
	int i;
	u8 *p8 = data;

	if (!debug_en || !data || len <= 0)
		return;

	ILI_DBG("dump %s (len=%d, type=%d):\n", name ? name : "", len, type);
	for (i = 0; i < len && i < 64; i++) {
		if ((i & 15) == 0)
			printk(KERN_CONT "ILITEK: ");
		printk(KERN_CONT "%02x ", p8[i]);
		if ((i & 15) == 15 || i == len - 1)
			printk(KERN_CONT "\n");
	}
}

int ili_ice_mode_write(u32 addr, u32 data, int len)
{
	int ret = 0, i;
	u8 txbuf[64] = { 0 };

	if (!atomic_read(&ilits->ice_stat)) {
		ILI_ERR("ice mode not enabled\n");
		return -EINVAL;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (u8)((addr & 0x000000FF) >> 0);
	txbuf[2] = (u8)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (u8)((addr & 0x00FF0000) >> 16);

	for (i = 0; i < len; i++)
		txbuf[i + 4] = (u8)(data >> (8 * i));

	ret = ilits->wrapper(txbuf, len + 4, NULL, 0, false, false);
	if (ret < 0)
		ILI_ERR("Failed to write data in ice mode, ret = %d\n", ret);

	return ret;
}

int ili_ice_mode_read(u32 addr, u32 *data, int len)
{
	int ret = 0;
	u8 rxbuf[4] = { 0 };
	u8 txbuf[4] = { 0 };

	if (len > sizeof(rxbuf)) {
		ILI_ERR("ice mode read length %d exceeds 4\n", len);
		return -EINVAL;
	}

	if (!atomic_read(&ilits->ice_stat)) {
		ILI_ERR("ice mode not enabled\n");
		return -EINVAL;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (u8)((addr & 0x000000FF) >> 0);
	txbuf[2] = (u8)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (u8)((addr & 0x00FF0000) >> 16);

	ret = ilits->wrapper(txbuf, sizeof(txbuf), NULL, 0, false, false);
	if (ret < 0)
		goto out;

	ret = ilits->wrapper(NULL, 0, rxbuf, len, false, false);
	if (ret < 0)
		goto out;

	if (len == 1)
		*data = rxbuf[0];
	else if (len == 2)
		*data = (rxbuf[0] | (rxbuf[1] << 8));
	else if (len == 3)
		*data = (rxbuf[0] | (rxbuf[1] << 8) | (rxbuf[2] << 16));
	else
		*data = (rxbuf[0] | (rxbuf[1] << 8) | (rxbuf[2] << 16) | (rxbuf[3] << 24));

out:
	if (ret < 0)
		ILI_ERR("Failed to read data in ice mode, ret = %d\n", ret);

	return ret;
}

int ili_ice_mode_write_by_mode(u32 addr, u32 data, int len, int mode)
{
	if (mode == SLAVE)
		return ili_mspi_write_slave_register(addr, data, len);
	return ili_ice_mode_write(addr, data, len);
}

int ili_ice_mode_read_by_mode(u32 addr, u32 *data, int len, int mode)
{
	if (mode == SLAVE) {
		ili_mspi_read_slave_register(addr, data, len);
		return 0;
	}
	return ili_ice_mode_read(addr, data, len);
}

int ili_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value)
{
	u32 data = 0;

	if (ili_ice_mode_read(addr, &data, sizeof(u32)) < 0) {
		ILI_ERR("Read data error\n");
		return -EIO;
	}

	data &= (~mask);
	data |= (value & mask);

	ILI_DBG("mask value data = %x\n", data);

	if (ili_ice_mode_write(addr, data, sizeof(u32)) < 0) {
		ILI_ERR("Failed to re-write data in ICE mode\n");
		return -EIO;
	}
	return 0;
}

void ili_cascade_sync_ctrl(bool mode)
{
	u8 cmd[2] = { 0xF0, mode ? 0x01 : 0x00 };

	if (ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false) < 0)
		ILI_ERR("Failed to set cascade sync ctrl\n");
}

int ili_ice_mode_ctrl(bool enable, bool mcu)
{
	int ret = 0;
	u8 cmd_open[4] = { 0x25, 0x62, 0x10, 0x18 };
	u8 cmd_close[4] = { 0x1B, 0x62, 0x10, 0x18 };

	ILI_INFO("%s ICE mode, mcu on = %d\n", (enable ? "Enable" : "Disable"), mcu);

	if (enable) {
		if (atomic_read(&ilits->ice_stat)) {
			ILI_INFO("ice mode already enabled\n");
			return 0;
		}
		if (mcu)
			cmd_open[0] = 0x1F;
		atomic_set(&ilits->ice_stat, ENABLE);
		if (ilits->wrapper(cmd_open, sizeof(cmd_open), NULL, 0, false, false) < 0) {
			ILI_ERR("write ice mode cmd error\n");
			atomic_set(&ilits->ice_stat, DISABLE);
			return -EIO;
		}
		ili_core_spi_setup(1);
	} else {
		if (!atomic_read(&ilits->ice_stat)) {
			ILI_INFO("ice mode already disabled\n");
			return 0;
		}
		ret = ilits->wrapper(cmd_close, sizeof(cmd_close), NULL, 0, false, false);
		if (ret < 0) {
			ILI_ERR("Exit to ICE Mode failed !!\n");
			atomic_set(&ilits->ice_stat, ENABLE);
		} else {
			atomic_set(&ilits->ice_stat, DISABLE);
			ili_core_spi_setup(7);
		}
	}

	return ret;
}

int ili_ice_mode_ctrl_by_mode_spi(bool enable, bool mcu, int mode)
{
	return ili_ice_mode_ctrl(enable, mcu);
}

int ili_ice_mode_ctrl_by_mode_i2c(bool enable, bool mcu, int mode)
{
	return ili_ice_mode_ctrl(enable, mcu);
}

int ili_ice_mode_ctrl_by_mode(bool enable, bool mcu, int mode)
{
	if (ilits->ice_mode_ctrl)
		return ilits->ice_mode_ctrl(enable, mcu, mode);
	return ili_ice_mode_ctrl(enable, mcu);
}

int ili_set_bypass_mode(bool mcu)
{
	return ili_ice_mode_ctrl(ENABLE, mcu);
}

void ili_mspi_set_slave_to_qual_mode(void)
{
	ILI_DBG("set slave to qual mode\n");
}

int ili_mspi_write_slave_register(u32 addr, u32 data, int len)
{
	return ili_ice_mode_write(addr, data, len);
}

void ili_mspi_read_slave_register(u32 addr, u32 *data, int len)
{
	ili_ice_mode_read(addr, data, len);
}

int ili_spi_ms_register_read(u32 addr, u32 *data, int len)
{
	return ili_ice_mode_read(addr, data, len);
}

int ili_spi_ice_mode_read(u32 addr, u32 *data, int len)
{
	return ili_ice_mode_read(addr, data, len);
}

int ili_wdt_reset_status(bool enter_ice, bool exit_ice)
{
	int ret = 0;

	if (enter_ice)
		ili_ice_mode_ctrl(ENABLE, false);

	ret = ili_ice_mode_write(WDT_ADDR, 0x00, 1);
	if (ret < 0)
		ILI_ERR("Failed to clear WDT status\n");

	if (exit_ice)
		ili_ice_mode_ctrl(DISABLE, false);

	return ret;
}

void ili_ic_edge_palm_para_init(void)
{
}

void ili_ic_send_edge_palm_para(void)
{
}

static int ili_ic_func_ctrl_common(const char *name, int ctrl)
{
	int i = 0, ret;

	for (i = 0; i < ARRAY_SIZE(func_ctrl); i++) {
		if (strlen(name) == strlen(func_ctrl[i].name) &&
		    strncmp(name, func_ctrl[i].name, strlen(name)) == 0) {
			if (ilits->protocol->ver == PROTOCOL_VER_500) {
				ILI_ERR("Non support function ctrl with protocol v5.0\n");
				return -EINVAL;
			}

			if (ilits->protocol->ver >= PROTOCOL_VER_560) {
				if (strncmp(func_ctrl[i].name, "gesture", strlen("gesture")) == 0 ||
				    strncmp(func_ctrl[i].name, "phone_cover_window",
					    strlen("phone_cover_window")) == 0) {
					ILI_INFO("Non support %s function ctrl\n", func_ctrl[i].name);
					return -EINVAL;
				}
			}

			func_ctrl[i].cmd[2] = (u8)ctrl;
			ILI_INFO("func = %s, len = %d, cmd = 0x%x, 0x%x, 0x%x\n",
				 func_ctrl[i].name, func_ctrl[i].len,
				 func_ctrl[i].cmd[0], func_ctrl[i].cmd[1], func_ctrl[i].cmd[2]);

			ret = ilits->wrapper(func_ctrl[i].cmd, func_ctrl[i].len,
					     NULL, 0, true, false);
			if (ret < 0) {
				ILI_ERR("Write func ctrl cmd failed\n");
				return ret;
			}

			if (func_ctrl[i].rec_state < 2) {
				if (ctrl == func_ctrl[i].def_cmd)
					func_ctrl[i].rec_state = DISABLE;
				else
					func_ctrl[i].rec_state = ENABLE;
				func_ctrl[i].rec_cmd = (u8)ctrl;
			}
			return 0;
		}
	}

	ILI_ERR("Not found function ctrl, %s\n", name);
	return -EINVAL;
}

int ili_ic_func_ctrl(const char *name, int ctrl)
{
	return ili_ic_func_ctrl_common(name, ctrl);
}

int ili_ic_func_ctrl_export(const char *name, int ctrl)
{
	int ret;

	if (!ilits)
		return -ENODEV;

	mutex_lock(&ilits->touch_mutex);
	ret = ili_ic_func_ctrl_common(name, ctrl);
	mutex_unlock(&ilits->touch_mutex);
	return ret;
}
EXPORT_SYMBOL(ili_ic_func_ctrl_export);

void ili_ic_func_ctrl_reset(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(func_ctrl); i++) {
		if (func_ctrl[i].rec_state == ENABLE) {
			func_ctrl[i].rec_state = DISABLE;
			func_ctrl[i].rec_cmd = 0xFF;
		}
	}
}

int ili_ic_code_reset(bool mcu)
{
	int ret;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed before code reset\n");
	}

	ret = ili_ice_mode_write(0x40040, 0xAE, 1);
	if (ret < 0)
		ILI_ERR("ic code reset failed\n");

	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed after code reset\n");
	}
	return ret;
}

int ili_get_dma1_config(u32 *reg_data)
{
	return ili_ice_mode_read(DMA1_BASE_ADDR, reg_data, sizeof(u32));
}

int ili_set_dma1_config(u32 reg_data)
{
	return ili_ice_mode_write(DMA1_BASE_ADDR, reg_data, sizeof(u32));
}

int ili_ic_whole_reset(bool mcu, bool withflash)
{
	int ret = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed before chip reset\n");
	}

	ILI_INFO("ic whole reset key = 0x%x, edge_delay = %d, withflash = %d\n",
		 ilits->chip->reset_key, ilits->rst_edge_delay, withflash);

	ret = ili_ice_mode_write(ilits->chip->reset_addr, ilits->chip->reset_key, sizeof(u32));
	if (ret < 0) {
		ILI_ERR("ic whole reset failed\n");
		goto out;
	}

	mdelay(10);

out:
	atomic_set(&ilits->ice_stat, DISABLE);
	ili_core_spi_setup(7);
	return ret;
}

int ili_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu)
{
	bool ice = atomic_read(&ilits->ice_stat);
	int ret = 0;

	if (!ice) {
		ret = ili_ice_mode_ctrl(ENABLE, mcu);
		if (ret < 0)
			return ret;
	}

	/* DDI page/reg write via ICE bridge */
	ili_ice_mode_write(0x048000, page, 1);
	ili_ice_mode_write(0x048004, reg, 1);
	ili_ice_mode_write(0x048008, data, 1);

	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed after writing ddi reg\n");
	}
	return ret;
}

int ili_ic_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu)
{
	bool ice = atomic_read(&ilits->ice_stat);
	u32 val = 0;
	int ret = 0;

	if (!ice) {
		ret = ili_ice_mode_ctrl(ENABLE, mcu);
		if (ret < 0)
			return ret;
	}

	ili_ice_mode_write(0x048000, page, 1);
	ili_ice_mode_write(0x048004, reg, 1);
	ret = ili_ice_mode_read(0x048008, &val, 1);
	if (ret == 0 && data)
		*data = (u8)(val & 0xFF);

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, mcu);

	return ret;
}

int ili_ic_set_cascade_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu)
{
	return ili_ic_set_ddi_reg_onepage(page, reg, data, mcu);
}

int ili_ic_get_cascade_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu)
{
	return ili_ic_get_ddi_reg_onepage(page, reg, data, mcu);
}

u32 ili_ic_get_pc_counter(int stat)
{
	bool ice = atomic_read(&ilits->ice_stat);
	u32 pc = 0, latch = 0;

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, false) < 0) {
			ILI_ERR("Enable ice mode failed while reading pc counter\n");
			return 0;
		}
	}

	ili_ice_mode_read(ilits->chip->pc_counter_addr, &pc, sizeof(u32));
	ili_ice_mode_read(ilits->chip->pc_latch_addr, &latch, sizeof(u32));
	ILI_ERR("Read Master counter (addr: 0x%x) = 0x%x, latch (addr: 0x%x) = 0x%x\n",
		ilits->chip->pc_counter_addr, pc, ilits->chip->pc_latch_addr, latch);

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, false);

	return pc;
}

int ili_ic_int_trigger_ctrl(bool pulse)
{
	if (ili_ic_func_ctrl("int_trigger", pulse ? 1 : 0) < 0) {
		ILI_ERR("Write CMD error, set back to pulse\n");
		return -EIO;
	}
	ilits->int_pulse = pulse;
	ILI_INFO("INT Trigger = %s\n", pulse ? "Pulse" : "Level");
	return 0;
}

int ili_ic_check_int_level(bool level)
{
	int timer = 3000;
	int gpio = ilits->tp_int;

	while (--timer > 0) {
		if (level) {
			if (gpio_get_value(gpio)) {
				ILI_DBG("INT high detected.\n");
				return 0;
			}
		} else {
			if (!gpio_get_value(gpio)) {
				ILI_DBG("INT low detected.\n");
				return 0;
			}
		}
		mdelay(1);
	}
	ILI_ERR("Error! INT level no detected.\n");
	return -ETIMEDOUT;
}

int ili_ic_check_int_pulse(bool pulse)
{
	long ret;

	/* cmd_int_check was already set to ENABLE by ili_spi_wrapper before TX */
	ret = wait_event_interruptible_timeout(ilits->inq,
					       atomic_read(&ilits->cmd_int_check) == DISABLE,
					       msecs_to_jiffies(ilits->wait_int_timeout));
	if (ret <= 0) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_ERR("Error! INT pulse timeout.\n");
		return -ETIMEDOUT;
	}
	return 0;
}

int ili_ic_check_busy(int count, int delay, bool irq)
{
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_CDC_BUSY_STATE };
	u8 busy = 0, rby = 0;

	if (ilits->actual_tp_mode == P5_X_FW_AP_MODE)
		rby = 0x41;
	else if (ilits->actual_tp_mode == P5_X_FW_TEST_MODE)
		rby = 0x51;
	else {
		ILI_ERR("Unknown TP mode (0x%x)\n", ilits->actual_tp_mode);
		return -EINVAL;
	}

	ILI_INFO("check busy: rby = 0x%x, delay = %d\n", rby, delay);

	do {
		if (ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false) < 0) {
			ILI_ERR("Write check busy cmd failed\n");
			return -EIO;
		}

		if (ilits->wrapper(&cmd[1], sizeof(u8), &busy, sizeof(u8), irq, false) < 0) {
			ILI_ERR("Read check busy failed\n");
			return -EIO;
		}

		ILI_DBG("busy = 0x%x\n", busy);
		if (busy == rby) {
			ILI_INFO("Check busy free\n");
			return 0;
		}
		mdelay(delay);
	} while (--count > 0);

	ILI_ERR("Check busy (0x%x) timeout !\n", busy);
	ili_ic_get_pc_counter(0);
	return -ETIMEDOUT;
}

int ili_ic_get_project_id(u8 *pdata, int size)
{
	if (!pdata || size <= 0)
		return -EINVAL;
	memset(pdata, 0, size);
	return 0;
}

int ili_ic_get_core_ver(void)
{
	int ret = 0;
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_GET_CORE_VERSION_NEW };
	u8 buf[10] = { 0 };

	if (ilits->protocol->ver < PROTOCOL_VER_540)
		cmd[1] = P5_X_GET_CORE_VERSION;

	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false);
	if (ret < 0) {
		ILI_ERR("Write core ver (0x%x) failed\n", cmd[1]);
		return ret;
	}

	ret = ilits->wrapper(&cmd[1], sizeof(u8), buf, ilits->protocol->core_ver_len, true, false);
	if (ret < 0) {
		ILI_ERR("Read core ver failed\n");
		return ret;
	}

	ilits->chip->core_ver = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	ILI_INFO("Core version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	return 0;
}

int ili_fw_uart_ctrl(u8 ctrl)
{
	u8 cmd[4] = { P5_X_I2C_UART, 0x03, 0x00, ctrl };

	if (ctrl > 1) {
		ILI_INFO("Unknown cmd, ignore\n");
		return -EINVAL;
	}
	ILI_INFO("%s UART mode\n", ctrl ? "Enable" : "Disable");
	return ilits->wrapper(cmd, sizeof(cmd), NULL, 0, true, false);
}

int ili_ic_get_fw_ver(void)
{
	int ret = 0;
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_GET_FW_VERSION };
	u8 buf[10] = { 0 };

	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false);
	if (ret < 0) {
		ILI_ERR("Write fw ver cmd failed\n");
		return ret;
	}

	ret = ilits->wrapper(&cmd[1], sizeof(u8), buf, ilits->protocol->fw_ver_len, true, false);
	if (ret < 0) {
		ILI_ERR("Read fw ver failed\n");
		return ret;
	}

	if (buf[0] != P5_X_GET_FW_VERSION) {
		ILI_ERR("Invalid firmware ver\n");
		return -EINVAL;
	}

	ilits->chip->fw_ver = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	ilits->chip->fw_mp_ver = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
	ILI_INFO("Firmware version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	return 0;
}

int ili_ic_get_panel_info(void)
{
	int ret = 0;
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_GET_PANEL_INFORMATION };
	u8 rcmd = P5_X_GET_REPORT_INFORMATION;
	u8 buf[16] = { 0 };
	u8 rbuf[4] = { 0 };
	int rlen = ilits->protocol->panel_info_len;

	if (rlen > sizeof(buf))
		rlen = sizeof(buf);

	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false);
	if (ret < 0)
		return ret;

	ret = ilits->wrapper(&cmd[1], sizeof(u8), buf, rlen, true, false);
	if (ret < 0)
		return ret;

	if (buf[0] == P5_X_GET_PANEL_INFORMATION) {
		ilits->panel_wid = (buf[1] << 8) | buf[2];
		ilits->panel_hei = (buf[3] << 8) | buf[4];
	}

	if (ilits->panel_wid == 0 || ilits->panel_hei == 0) {
		ilits->panel_wid = TOUCH_SCREEN_X_MAX;
		ilits->panel_hei = TOUCH_SCREEN_Y_MAX;
	}

	if (ilits->panel_wid > TOUCH_SCREEN_X_MAX)
		ilits->trans_xy = true;

	if (ilits->wrapper(&rcmd, 1, rbuf, 2, true, false) == 0 &&
	    rbuf[0] == P5_X_GET_REPORT_INFORMATION) {
		ilits->rib.nReportResolutionMode = rbuf[1] & 0x7;
	}

	ILI_INFO("Panel info: width = %d, height = %d, trans_xy = %d, res_mode = %d\n",
		 ilits->panel_wid, ilits->panel_hei, ilits->trans_xy,
		 ilits->rib.nReportResolutionMode);
	return 0;
}

int ili_ic_get_tp_info(void)
{
	int ret = 0;
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_GET_TP_INFORMATION };
	u8 buf[20] = { 0 };

	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false);
	if (ret < 0)
		return ret;

	ret = ilits->wrapper(&cmd[1], sizeof(u8), buf, ilits->protocol->tp_info_len, true, false);
	if (ret < 0)
		return ret;

	if (buf[0] != P5_X_GET_TP_INFORMATION)
		return -EINVAL;

	ilits->min_x = buf[1];
	ilits->min_y = buf[2];
	ilits->max_x = (buf[4] << 8) | buf[3];
	ilits->max_y = (buf[6] << 8) | buf[5];
	ilits->xch_num = buf[7];
	ilits->ych_num = buf[8];
	ilits->stx = buf[11];
	ilits->srx = buf[12];

	ILI_INFO("TP Info: min_x = %d, min_y = %d, max_x = %d, max_y = %d\n",
		 ilits->min_x, ilits->min_y, ilits->max_x, ilits->max_y);
	ILI_INFO("TP Info: xch = %d, ych = %d, stx = %d, srx = %d\n",
		 ilits->xch_num, ilits->ych_num, ilits->stx, ilits->srx);
	return 0;
}

int ili_ic_get_compress_info(void)
{
	return 0;
}

int ili_ic_get_all_info(void)
{
	ili_ic_get_protocl_ver();
	ili_ic_get_fw_ver();
	ili_ic_get_core_ver();
	ili_ic_get_tp_info();
	ili_ic_get_panel_info();
	return 0;
}

int ili_ic_get_protocl_ver(void)
{
	int i, ret = 0;
	u8 cmd[2] = { P5_X_READ_DATA_CTRL, P5_X_GET_PROTOCOL_VERSION };
	u8 buf[10] = { 0 };
	u32 ver;

	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false);
	if (ret < 0)
		return ret;

	ret = ilits->wrapper(&cmd[1], sizeof(u8), buf, ilits->protocol->pro_ver_len, true, false);
	if (ret < 0)
		return ret;

	if (buf[0] != P5_X_GET_PROTOCOL_VERSION)
		return -EINVAL;

	ver = (buf[1] << 16) | (buf[2] << 8) | buf[3];
	ILI_INFO("Protocol version = %d.%d.%d\n", buf[1], buf[2], buf[3]);

	for (i = 0; i < PROTOCL_VER_NUM; i++) {
		if (ver == protocol_info[i].ver) {
			ilits->protocol = &protocol_info[i];
			return 0;
		}
	}

	ILI_ERR("Not found a correct protocol version in list, use newest version\n");
	ilits->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
	return 0;
}

int ili_ic_get_info(void)
{
	if (!atomic_read(&ilits->ice_stat)) {
		ILI_ERR("ice mode doesn't enable\n");
		return -EINVAL;
	}

	ili_ice_mode_read(ilits->chip->pid_addr, &ilits->chip->pid, sizeof(u32));
	ili_ice_mode_read(ilits->chip->otp_addr, &ilits->chip->otp_id, sizeof(u32));
	ili_ice_mode_read(ilits->chip->ana_addr, &ilits->chip->ana_id, sizeof(u32));

	ilits->chip->id = (u16)(ilits->chip->pid >> 16);
	ilits->chip->type = (u8)((ilits->chip->pid & 0x0000FF00) >> 8);
	ilits->chip->ver = (u8)(ilits->chip->pid & 0x000000FF);

	ILI_INFO("CHIP: PID = 0x%x, ID = 0x%x, TYPE = 0x%x, VER = 0x%x\n",
		 ilits->chip->pid, ilits->chip->id, ilits->chip->type, ilits->chip->ver);

	if (ilits->chip->id == ILI9882_CHIP) {
		u8 ddi_val = 0;

		ili_ic_get_ddi_reg_onepage(0x06, 0xF3, &ddi_val, false);
		ILI_INFO("9882 ddi reg = 0x%x\n", ddi_val);
	}

	return 0;
}

int ili_ic_dummy_check(void)
{
	u32 wdata = 0xA55A5AA5, rdata = 0;
	int ret;

	if (!atomic_read(&ilits->ice_stat))
		return -EINVAL;

	ret = ili_ice_mode_write(0x040020, wdata, sizeof(u32));
	if (ret < 0)
		return ret;
	ret = ili_ice_mode_read(0x040020, &rdata, sizeof(u32));
	if (ret < 0)
		return ret;

	return (rdata == wdata) ? 0 : -EIO;
}

int ili_tddi_ic_sram_test(void)
{
	int ret = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		ret = ili_ice_mode_ctrl(ENABLE, false);
		if (ret < 0)
			return ret;
	}

	ret = ili_ic_dummy_check();
	if (ret < 0)
		ILI_INFO("SRAM TEST FAIL\n");
	else
		ILI_INFO("SRAM TEST PASS\n");

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, false);
	return ret;
}

int ili_move_mp_code_iram(void)
{
	ILI_INFO("Download MP code to iram\n");
	return ili_fw_upgrade_handler(NULL);
}

int ili_proximity_near(int mode)
{
	ilits->prox_near = true;
	return ili_ic_func_ctrl("proximity", 1);
}

int ili_proximity_far(int mode)
{
	ilits->prox_near = false;
	return ili_ic_func_ctrl("proximity", 0);
}

int ili_set_gesture_symbol(void)
{
	u8 cmd[7] = { 0 };
	struct gesture_symbol *ptr_sym = &ilits->ges_sym;
	u8 *ptr = (u8 *)ptr_sym;

	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0x01;
	cmd[2] = 0x0A;
	cmd[3] = 0x08;
	cmd[4] = ptr[0];
	cmd[5] = ptr[1];
	cmd[6] = ptr[2];

	ili_dump_data(cmd, 8, sizeof(cmd), 0, "gesture symbol");
	if (ilits->wrapper(cmd, 2, NULL, 0, true, false) < 0)
		return -EIO;
	if (ilits->wrapper(&cmd[1], sizeof(cmd) - 1, NULL, 0, true, false) < 0)
		return -EIO;

	return 0;
}

int ili_move_gesture_code_flash(int mode)
{
	int ret = 0;

	ILI_INFO("Gesture mode = %d (flash)\n", mode);
	if (ili_set_tp_data_len(mode, true, NULL) < 0)
		return -EIO;
	if (ili_ic_func_ctrl("lpwg", 0x02) < 0)
		return -EIO;
	ret = ili_set_gesture_symbol();
	return ret;
}

int ili_move_gesture_code_iram(int mode)
{
	int ret = 0, retry = 10;
	u8 data = 3;
	u8 pre_cmd[2] = { P5_X_READ_DATA_CTRL, 0x01 };
	u8 rdy_pre[2] = { P5_X_READ_DATA_CTRL, 0x0A };
	u8 rdy_cmd[3] = { 0x01, 0x0A, 0x05 };
	u8 ready = 0;

	ILI_INFO("Gesture mode = %d (iram)\n", mode);

	/* 1. Notify AP firmware to prepare for gesture code load */
	ilits->wrapper(pre_cmd, sizeof(pre_cmd), NULL, 0, false, false);
	if (ili_ic_func_ctrl("lpwg", 0x03) < 0)
		ILI_ERR("write lpwg 0x03 failed\n");

	/* 2. Set gesture data len (221), gesture symbols, and send lpwg 0x02 */
	if (ili_set_tp_data_len(mode, true, &data) < 0)
		ILI_ERR("Failed to set tp data length\n");

	/* 3. Poll AP firmware until gesture ready byte (0x91) */
	ili_irq_enable();
	while (retry-- > 0) {
		ilits->wrapper(rdy_pre, sizeof(rdy_pre), NULL, 0, false, false);
		if (ilits->wrapper(rdy_cmd, sizeof(rdy_cmd), &ready, 1, true, false) == 0 &&
		    ready == 0x91) {
			ILI_INFO("Ready to load gesture code (0x%x)\n", ready);
			break;
		}
		mdelay(10);
	}
	ili_irq_disable();

	/* 4. Load gesture code block into IRAM (0x11f00) without resetting AP */
	ilits->gesture_load_code = true;
	ret = ili_fw_upgrade_handler(NULL);
	ilits->gesture_load_code = false;
	if (ret < 0) {
		ILI_ERR("FW upgrade failed during gesture mode\n");
		return ret;
	}

	mdelay(20);

	/* 5. Tell AP firmware gesture code is loaded and enter gesture detection */
	if (ili_ic_func_ctrl("lpwg", 0x06) < 0)
		ILI_ERR("write lpwg 0x06 failed\n");

	return 0;
}

void ili_ic_init(void)
{
	memset(&chip, 0, sizeof(chip));
	chip.pid_addr = PID_ADDR;
	chip.otp_addr = OTP_ID_ADDR;
	chip.ana_addr = ANA_ID_ADDR;
	chip.pc_counter_addr = PC_COUNTER_ADDR;
	chip.pc_latch_addr = PC_LATCH_ADDR;
	chip.reset_addr = CHIP_RESET_ADDR;
	chip.reset_key = 0xa0019878;
	chip.wtd_key = 0x5A;
	chip.no_bk_shift = 8192;
	chip.max_count = 0x1FFFF;

	ilits->chip = &chip;
	ilits->protocol = &protocol_info[PROTOCL_VER_NUM - 1];

	/* Enable all hardware gesture symbols; tran_touch filters via /proc/gesture_function */
	ilits->ges_sym.double_tap = 1;
	ilits->ges_sym.alphabet_line_2_top = 1;
	ilits->ges_sym.alphabet_line_2_bottom = 1;
	ilits->ges_sym.alphabet_line_2_left = 1;
	ilits->ges_sym.alphabet_line_2_right = 1;
	ilits->ges_sym.alphabet_m = 1;
	ilits->ges_sym.alphabet_w = 1;
	ilits->ges_sym.alphabet_c = 1;
	ilits->ges_sym.alphabet_E = 1;
	ilits->ges_sym.alphabet_V = 1;
	ilits->ges_sym.alphabet_O = 1;
	ilits->ges_sym.alphabet_S = 1;
	ilits->ges_sym.alphabet_Z = 1;
}
