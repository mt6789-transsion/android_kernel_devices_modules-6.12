// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * FocalTech FT8725 TXD FHD+ 120Hz DSI video mode panel driver.
 */

#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk_panel_ext.h"
#include "tran_drm_panel_i2c.h"
#include "tran_touch.h"

#define REGFLAG_DELAY        0xfffc
#define REGFLAG_END_OF_TABLE 0xfffd

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;
	int error;
};

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[120];
};

static bool tran_is_lcm_poweroff;
int tran_new_gesture_fun_enable;

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static struct LCM_setting_table lcm_initialization_setting[239] = {
	{ 0x00, 1, { 0x00 } },
	{ 0xff, 3, { 0x87, 0x25, 0x01 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xff, 2, { 0x87, 0x25 } },
	{ 0x00, 1, { 0xa3 } },
	{ 0xb3, 2, { 0x09, 0x9c } },
	{ 0x00, 1, { 0x80 } },
	{ 0xc0, 6, { 0x00, 0x49, 0x00, 0x28, 0x00, 0x12 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xc0, 6, { 0x00, 0x49, 0x00, 0x28, 0x00, 0x12 } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xc0, 6, { 0x00, 0x5d, 0x00, 0x28, 0x00, 0x12 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xc0, 5, { 0x00, 0xc9, 0x00, 0x28, 0x12 } },
	{ 0x00, 1, { 0xc1 } },
	{ 0xc0, 8, { 0x00, 0x94, 0x00, 0x81, 0x00, 0x63, 0x00, 0xc1 } },
	{ 0x00, 1, { 0x70 } },
	{ 0xc0, 6, { 0x00, 0x92, 0x00, 0x28, 0x00, 0x12 } },
	{ 0x00, 1, { 0xa3 } },
	{ 0xc1, 6, { 0x00, 0x54, 0x00, 0x32, 0x00, 0x02 } },
	{ 0x00, 1, { 0xb7 } },
	{ 0xc1, 2, { 0x00, 0x48 } },
	{ 0x00, 1, { 0x7b } },
	{ 0xce, 2, { 0xff, 0xff } },
	{ 0x00, 1, { 0x80 } },
	{ 0xce, 16, { 0x01, 0x81, 0x17, 0x1a, 0x00, 0xc8, 0x00, 0xcd, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x8c, 0x00, 0x87 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xce, 15, { 0x00, 0xb7, 0x10, 0xae, 0x00, 0xb7, 0x80, 0xff, 0xff, 0x00, 0x0c, 0x1c, 0x0e, 0x0a, 0x0a } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xce, 3, { 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xce, 3, { 0x22, 0x00, 0x00 } },
	{ 0x00, 1, { 0xd1 } },
	{ 0xce, 7, { 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0xe1 } },
	{ 0xce, 11, { 0x05, 0x01, 0xca, 0x02, 0xfb, 0x02, 0xfb, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0xf1 } },
	{ 0xce, 9, { 0x28, 0x1e, 0x14, 0x00, 0xe9, 0x00, 0xe9, 0x00, 0xe9 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xcf, 4, { 0x00, 0x00, 0xbe, 0xc2 } },
	{ 0x00, 1, { 0xb5 } },
	{ 0xcf, 4, { 0x05, 0x05, 0x8c, 0x90 } },
	{ 0x00, 1, { 0xc0 } },
	{ 0xcf, 4, { 0x09, 0x09, 0x97, 0x9b } },
	{ 0x00, 1, { 0xc5 } },
	{ 0xcf, 4, { 0x09, 0x09, 0x9d, 0xa1 } },
	{ 0x00, 1, { 0x60 } },
	{ 0xcf, 8, { 0x00, 0x00, 0x84, 0x88, 0x05, 0x05, 0x43, 0x47 } },
	{ 0x00, 1, { 0x70 } },
	{ 0xcf, 8, { 0x00, 0x00, 0x69, 0x6d, 0x05, 0x05, 0x19, 0x1d } },
	{ 0x00, 1, { 0xd1 } },
	{ 0xc1, 12, { 0x0b, 0x60, 0x0f, 0xdc, 0x1b, 0x15, 0x05, 0xaf, 0x07, 0xe5, 0x0d, 0x81 } },
	{ 0x00, 1, { 0xe1 } },
	{ 0xc1, 2, { 0x0f, 0xdc } },
	{ 0x00, 1, { 0xe4 } },
	{ 0xcf, 12, { 0x0a, 0x28, 0x0a, 0x27, 0x0a, 0x27, 0x0a, 0x27, 0x0a, 0x27, 0x0a, 0x27 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xc1, 2, { 0x44, 0x44 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xc1, 1, { 0x03 } },
	{ 0x00, 1, { 0xf5 } },
	{ 0xcf, 1, { 0x00 } },
	{ 0x00, 1, { 0xf6 } },
	{ 0xcf, 1, { 0x78 } },
	{ 0x00, 1, { 0xf1 } },
	{ 0xcf, 1, { 0x78 } },
	{ 0x00, 1, { 0x91 } },
	{ 0xc4, 1, { 0x88 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xc5, 2, { 0x87, 0x59 } },
	{ 0x00, 1, { 0x93 } },
	{ 0xc1, 1, { 0x82 } },
	{ 0x00, 1, { 0x9e } },
	{ 0xc5, 1, { 0x87 } },
	{ 0x00, 1, { 0x88 } },
	{ 0xc4, 1, { 0x08 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xc2, 8, { 0x81, 0x00, 0x02, 0x8f, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xc2, 4, { 0x80, 0x00, 0x04, 0x8f } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xc2, 15, { 0x00, 0x04, 0x00, 0x04, 0x8f, 0x01, 0x05, 0x00, 0x04, 0x8f, 0x02, 0x06, 0x00, 0x04, 0x8f } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xc2, 10, { 0x03, 0x04, 0x00, 0x04, 0x8f, 0x04, 0x05, 0x00, 0x04, 0x8f } },
	{ 0x00, 1, { 0xe0 } },
	{ 0xc2, 4, { 0x44, 0x44, 0x40, 0x00 } },
	{ 0x00, 1, { 0xe8 } },
	{ 0xc2, 8, { 0x0b, 0x00, 0x05, 0x03, 0x02, 0x8a, 0x00, 0x00 } },
	{ 0x00, 1, { 0xd0 } },
	{ 0xc3, 16, { 0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0xe0 } },
	{ 0xc3, 16, { 0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xcb, 16, { 0x00, 0x01, 0x01, 0x01, 0xfd, 0xfd, 0x00, 0xfd, 0x03, 0xfe, 0xfd, 0x01, 0x00, 0x00, 0x00, 0x03 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xcb, 16, { 0x00, 0x00, 0x0f, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x7f, 0x00, 0x00, 0x00, 0xff } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xcb, 8, { 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xcb, 4, { 0x50, 0xd1, 0xe5, 0x00 } },
	{ 0x00, 1, { 0xc0 } },
	{ 0xcb, 4, { 0x40, 0x00, 0xf1, 0x00 } },
	{ 0x00, 1, { 0xd5 } },
	{ 0xcb, 11, { 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00 } },
	{ 0x00, 1, { 0xe0 } },
	{ 0xcb, 13, { 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xcc, 16, { 0x26, 0x22, 0x22, 0x24, 0x25, 0x22, 0x02, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x18, 0x18, 0x17, 0x17 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xcc, 8, { 0x16, 0x16, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x19 } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xcc, 16, { 0x26, 0x22, 0x22, 0x24, 0x25, 0x22, 0x02, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x18, 0x18, 0x17, 0x17 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xcc, 8, { 0x16, 0x16, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x19 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xcd, 16, { 0x26, 0x22, 0x22, 0x24, 0x25, 0x22, 0x02, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x18, 0x18, 0x17, 0x17 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xcd, 8, { 0x16, 0x16, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x19 } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xcd, 16, { 0x26, 0x22, 0x22, 0x24, 0x25, 0x22, 0x02, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x18, 0x18, 0x17, 0x17 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xcd, 8, { 0x16, 0x16, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x19 } },
	{ 0x00, 1, { 0x86 } },
	{ 0xc0, 8, { 0x00, 0x02, 0x00, 0x01, 0x0f, 0x0f, 0x0f, 0x03 } },
	{ 0x00, 1, { 0x96 } },
	{ 0xc0, 8, { 0x00, 0x00, 0x00, 0x02, 0x11, 0x11, 0x11, 0x04 } },
	{ 0x00, 1, { 0xa6 } },
	{ 0xc0, 8, { 0x00, 0x00, 0x00, 0x03, 0x16, 0x16, 0x16, 0x01 } },
	{ 0x00, 1, { 0xa3 } },
	{ 0xce, 6, { 0x00, 0x02, 0x00, 0x01, 0x0f, 0x03 } },
	{ 0x00, 1, { 0xb3 } },
	{ 0xce, 6, { 0x00, 0x02, 0x00, 0x01, 0x0f, 0x03 } },
	{ 0x00, 1, { 0x76 } },
	{ 0xc0, 8, { 0x00, 0x00, 0x00, 0x01, 0x29, 0x29, 0x29, 0x07 } },
	{ 0x00, 1, { 0x82 } },
	{ 0xa7, 2, { 0x10, 0x00 } },
	{ 0x00, 1, { 0x8d } },
	{ 0xa7, 1, { 0x01 } },
	{ 0x00, 1, { 0x8f } },
	{ 0xa7, 1, { 0x01 } },
	{ 0x00, 1, { 0x93 } },
	{ 0xc5, 1, { 0x2d } },
	{ 0x00, 1, { 0x97 } },
	{ 0xc5, 1, { 0x2d } },
	{ 0x00, 1, { 0x9a } },
	{ 0xc5, 1, { 0x19 } },
	{ 0x00, 1, { 0x9c } },
	{ 0xc5, 1, { 0x19 } },
	{ 0x00, 1, { 0xb6 } },
	{ 0xc5, 4, { 0x23, 0x23, 0x0f, 0x0f } },
	{ 0x00, 1, { 0x00 } },
	{ 0xd8, 2, { 0x2d, 0x2d } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xc3, 1, { 0x00 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xa7, 1, { 0x03 } },
	{ 0x00, 1, { 0xb1 } },
	{ 0xf5, 1, { 0x1f } },
	{ 0x00, 1, { 0xcb } },
	{ 0xc0, 1, { 0x11 } },
	{ 0x00, 1, { 0x9a } },
	{ 0xc4, 2, { 0x11, 0x08 } },
	{ 0x00, 1, { 0x93 } },
	{ 0xe9, 3, { 0xff, 0xff, 0xb0 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xa4, 1, { 0xc9 } },
	{ 0x00, 1, { 0x82 } },
	{ 0xf5, 1, { 0x01 } },
	{ 0x00, 1, { 0x93 } },
	{ 0xf5, 1, { 0x01 } },
	{ 0x00, 1, { 0x9b } },
	{ 0xf5, 1, { 0x49 } },
	{ 0x00, 1, { 0x9d } },
	{ 0xf5, 1, { 0x49 } },
	{ 0x00, 1, { 0x99 } },
	{ 0xcf, 1, { 0x50 } },
	{ 0x00, 1, { 0x9c } },
	{ 0xf5, 1, { 0x00 } },
	{ 0x00, 1, { 0x9e } },
	{ 0xf5, 1, { 0x00 } },
	{ 0x00, 1, { 0x87 } },
	{ 0xf5, 1, { 0x00 } },
	{ 0x00, 1, { 0xbe } },
	{ 0xc5, 2, { 0xc0, 0xc0 } },
	{ 0x00, 1, { 0x87 } },
	{ 0xc4, 1, { 0x08 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xc5, 6, { 0x10, 0x4a, 0x01, 0x1f, 0x4a, 0x00 } },
	{ 0x00, 1, { 0xe8 } },
	{ 0xc0, 1, { 0x40 } },
	{ 0x00, 1, { 0xe0 } },
	{ 0xcf, 1, { 0x34 } },
	{ 0x00, 1, { 0x92 } },
	{ 0xc5, 1, { 0x00 } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xc5, 1, { 0x40 } },
	{ 0x00, 1, { 0x98 } },
	{ 0xc5, 1, { 0x24 } },
	{ 0x00, 1, { 0xa1 } },
	{ 0xc5, 1, { 0x40 } },
	{ 0x00, 1, { 0x9d } },
	{ 0xc5, 1, { 0x24 } },
	{ 0x00, 1, { 0x94 } },
	{ 0xc5, 1, { 0x04 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xb3, 1, { 0x22 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xb3, 1, { 0x00 } },
	{ 0x00, 1, { 0x83 } },
	{ 0xb0, 1, { 0x63 } },
	{ 0x00, 1, { 0x87 } },
	{ 0xc5, 2, { 0x0a, 0x0a } },
	{ 0x00, 1, { 0xfc } },
	{ 0xc0, 2, { 0x00, 0x13 } },
	{ 0x00, 1, { 0xa0 } },
	{ 0xb0, 7, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x01 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xa7, 1, { 0x1d } },
	{ 0x00, 1, { 0x81 } },
	{ 0xa4, 1, { 0x83 } },
	{ 0x00, 1, { 0x90 } },
	{ 0xe9, 1, { 0x50 } },
	{ 0x00, 1, { 0xc0 } },
	{ 0xc5, 1, { 0x0f } },
	{ 0x00, 1, { 0x92 } },
	{ 0xcf, 1, { 0x12 } },
	{ 0x00, 1, { 0x84 } },
	{ 0xa4, 1, { 0x01 } },
	{ 0x00, 1, { 0x80 } },
	{ 0xa6, 2, { 0x00, 0x12 } },
	{ 0x00, 1, { 0xb0 } },
	{ 0xb4, 14, { 0x00, 0x14, 0x02, 0x00, 0x01, 0xe8, 0x00, 0x07, 0x05, 0x0e, 0x05, 0x16, 0x10, 0xf0 } },
	{ 0x00, 1, { 0x00 } },
	{ 0xff, 3, { 0xff, 0xff, 0xff } },
	{ 0x35, 1, { 0x00 } },
	{ 0x11, 0, {  } },
	{ REGFLAG_DELAY, 100, { } },
	{ 0x29, 0, {  } },
	{ REGFLAG_DELAY, 20, { } },
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[6] = {
	{ 0x28, 0, {  } },
	{ REGFLAG_DELAY, 20, { } },
	{ 0x10, 0, {  } },
	{ REGFLAG_DELAY, 100, { } },
	{ 0x00, 1, { 0x00 } },
	{ 0xf7, 4, { 0x5a, 0xa5, 0x95, 0x27 } },
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table,
		       unsigned int count)
{
	unsigned int i, j;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	for (i = 0; i < count; i++) {
		unsigned char buf[256] = { 0 };
		unsigned int cmd = table[i].cmd;

		if (cmd == REGFLAG_END_OF_TABLE)
			continue;
		if (cmd == REGFLAG_DELAY) {
			msleep(table[i].count);
			continue;
		}

		buf[0] = (unsigned char)cmd;
		if (table[i].count > 0) {
			for (j = 0; j < table[i].count; j++) {
				if (j >= 120)
					BUG();
				buf[j + 1] = table[i].para_list[j];
			}
		}

		if (ctx->error < 0)
			continue;

		if ((cmd & 0xff) <= 0xaf)
			ret = mipi_dsi_dcs_write_buffer(dsi, buf, table[i].count + 1);
		else
			ret = mipi_dsi_generic_write(dsi, buf, table[i].count + 1);

		if (ret < 0) {
			dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, buf);
			ctx->error = ret;
		}
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s Start\n", __func__);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset_gpio %ld\n",
		       __func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, lcm_initialization_setting,
		   ARRAY_SIZE(lcm_initialization_setting));
	pr_info("%s End\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	ctx->enabled = false;
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s Start\n", __func__);
	if (!ctx->prepared)
		return 0;

	push_table(ctx, lcm_deep_sleep_mode_in_setting,
		   ARRAY_SIZE(lcm_deep_sleep_mode_in_setting));

	tran_new_gesture_fun_enable = tran_call_tp_cmd_handle(TRAN_TP_CMD_GET_GESTURE_ENABLE, NULL);
	printk("ctp tran_new_gesture_fun_enable = %d\n", tran_new_gesture_fun_enable);

	if (!tran_new_gesture_fun_enable) {
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			pr_err("[error]%s: cannot get bias_neg %ld\n",
			       __func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		mdelay(5);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			pr_err("[error]%s: cannot get bias_pos %ld\n",
			       __func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		tran_is_lcm_poweroff = true;
	}

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("%s End\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s Start\n", __func__);
	if (ctx->prepared)
		return 0;

	if (tran_is_lcm_poweroff) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio))
			pr_err("[error]%s: cannot get reset_gpio %ld\n",
			       __func__, PTR_ERR(ctx->reset_gpio));
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(5);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			pr_err("[error]%s: cannot get bias_pos %ld\n",
			       __func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		udelay(200);

		tran_panel_i2c_write_bytes(0x00, 0x0f);
		tran_panel_i2c_write_bytes(0x01, 0x0f);
		mdelay(2);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			pr_err("[error]%s: cannot get bias_neg %ld\n",
			       __func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		tran_is_lcm_poweroff = false;
	}

	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	pr_info("%s End\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	ctx->enabled = true;
	return 0;
}

static const struct drm_display_mode switch_mode_120hz = {
	.clock = 341440,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1112,
	.htotal = 1130,
	.vdisplay = 2460,
	.vsync_start = 2498,
	.vsync_end = 2508,
	.vtotal = 2518,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock = 341712,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1112,
	.htotal = 1130,
	.vdisplay = 2460,
	.vsync_start = 3340,
	.vsync_end = 3350,
	.vtotal = 3360,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 341712,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1112,
	.htotal = 1130,
	.vdisplay = 2460,
	.vsync_start = 5020,
	.vsync_end = 5030,
	.vtotal = 5040,
};

static struct mtk_panel_params ext_params_120hz = {
	.data_rate = 874,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
	},
	.physical_width_um = 69174,
	.physical_height_um = 157563,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.output_mode = 1,
};

static struct mtk_panel_params ext_params_90hz = {
	.data_rate = 874,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
	},
	.physical_width_um = 69174,
	.physical_height_um = 157563,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.output_mode = 1,
};

static struct mtk_panel_params ext_params_60hz = {
	.data_rate = 874,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
	},
	.physical_width_um = 69174,
	.physical_height_um = 157563,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.output_mode = 1,
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
					unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
				   struct drm_connector *connector,
				   unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	int ret = 0;

	if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset_gpio %ld\n",
		       __func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = { 0x00, 0x00, 0x00 };
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x0a, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);
	if (data[0] == 0x9c)
		return 1;

	pr_info("ATA expect read data is %x %x %x\n", 0x9c, 0x02, 0x00);
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
};

static int lcm_get_modes(struct drm_panel *panel,
			 struct drm_connector *connector)
{
	struct drm_display_mode *mode_120, *mode_90, *mode_60;

	mode_120 = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_120) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120);
	mode_120->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_120);

	mode_90 = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_90) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_90);

	mode_60 = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_60);

	connector->display_info.width_mm = 69;
	connector->display_info.height_mm = 157;

	return 3;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	int ret;

	pr_info("%s+\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET;

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		pr_err("[error]%s: cannot get bias_pos %ld\n",
		       __func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		pr_err("[error]%s: cannot get bias_neg %ld\n",
		       __func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		pr_err("[error]%s: cannot get reset-gpios %ld\n",
		       __func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		pr_err("%s create new panel params fail\n", __func__);
	else
		pr_info("%s-\n", __func__);

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static void lcm_shutdown(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	pr_info("%s Start\n", __func__);
	if (!tran_is_lcm_poweroff) {
		pr_info("%s  lcm poweroff = %d, ctp_gesture = %d \n",
			__func__, tran_is_lcm_poweroff, tran_new_gesture_fun_enable);
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio))
			pr_err("[error]%s: cannot get reset_gpio %ld\n",
			       __func__, PTR_ERR(ctx->reset_gpio));
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		mdelay(2);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg))
			pr_err("[error]%s: cannot get bias_neg %ld\n",
			       __func__, PTR_ERR(ctx->bias_neg));
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		mdelay(5);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos))
			pr_err("[error]%s: cannot get bias_pos %ld\n",
			       __func__, PTR_ERR(ctx->bias_pos));
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		tran_is_lcm_poweroff = true;
	}
	pr_info("%s End\n", __func__);
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "ft8725,fhdp,dsi,vdo,txd,inx,120hz,x6837" },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
		.name = "ft8725_fhdp_dsi_vdo_txd_inx_120hz_x6837",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_DESCRIPTION("ft8725+inx dsi vdo DSC 120hz Panel Driver");
MODULE_LICENSE("GPL v2");
