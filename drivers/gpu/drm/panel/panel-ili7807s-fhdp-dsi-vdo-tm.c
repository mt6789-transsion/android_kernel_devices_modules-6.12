// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * Ilitek ILI7807S TM FHD+ 120Hz DSI video mode panel driver.
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
	unsigned char para_list[150];
};

static bool tran_is_lcm_poweroff;
int tran_new_gesture_fun_enable;

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static struct LCM_setting_table lcm_initialization_setting[407] = {
	{ 0xff, 3, { 0x78, 0x07, 0x01 } },
	{ 0x00, 1, { 0x44 } },
	{ 0x01, 1, { 0x03 } },
	{ 0x02, 1, { 0x27 } },
	{ 0x03, 1, { 0x2c } },
	{ 0x08, 1, { 0x82 } },
	{ 0x09, 1, { 0x04 } },
	{ 0x0a, 1, { 0x30 } },
	{ 0x0b, 1, { 0x00 } },
	{ 0x0c, 1, { 0x02 } },
	{ 0x0e, 1, { 0x08 } },
	{ 0x31, 1, { 0x07 } },
	{ 0x32, 1, { 0x07 } },
	{ 0x33, 1, { 0x07 } },
	{ 0x34, 1, { 0x34 } },
	{ 0x35, 1, { 0x35 } },
	{ 0x36, 1, { 0x36 } },
	{ 0x37, 1, { 0x07 } },
	{ 0x38, 1, { 0x07 } },
	{ 0x39, 1, { 0x07 } },
	{ 0x3a, 1, { 0x2e } },
	{ 0x3b, 1, { 0x2f } },
	{ 0x3c, 1, { 0x30 } },
	{ 0x3d, 1, { 0x40 } },
	{ 0x3e, 1, { 0x08 } },
	{ 0x3f, 1, { 0x13 } },
	{ 0x40, 1, { 0x11 } },
	{ 0x41, 1, { 0x12 } },
	{ 0x42, 1, { 0x10 } },
	{ 0x43, 1, { 0x00 } },
	{ 0x44, 1, { 0x2d } },
	{ 0x45, 1, { 0x07 } },
	{ 0x46, 1, { 0x07 } },
	{ 0x47, 1, { 0x28 } },
	{ 0x48, 1, { 0x41 } },
	{ 0x49, 1, { 0x07 } },
	{ 0x4a, 1, { 0x07 } },
	{ 0x4b, 1, { 0x07 } },
	{ 0x4c, 1, { 0x34 } },
	{ 0x4d, 1, { 0x35 } },
	{ 0x4e, 1, { 0x36 } },
	{ 0x4f, 1, { 0x07 } },
	{ 0x50, 1, { 0x07 } },
	{ 0x51, 1, { 0x07 } },
	{ 0x52, 1, { 0x2e } },
	{ 0x53, 1, { 0x2f } },
	{ 0x54, 1, { 0x30 } },
	{ 0x55, 1, { 0x40 } },
	{ 0x56, 1, { 0x08 } },
	{ 0x57, 1, { 0x13 } },
	{ 0x58, 1, { 0x11 } },
	{ 0x59, 1, { 0x12 } },
	{ 0x5a, 1, { 0x10 } },
	{ 0x5b, 1, { 0x00 } },
	{ 0x5c, 1, { 0x2d } },
	{ 0x5d, 1, { 0x07 } },
	{ 0x5e, 1, { 0x07 } },
	{ 0x5f, 1, { 0x28 } },
	{ 0x60, 1, { 0x41 } },
	{ 0x61, 1, { 0x07 } },
	{ 0x62, 1, { 0x07 } },
	{ 0x63, 1, { 0x07 } },
	{ 0x64, 1, { 0x34 } },
	{ 0x65, 1, { 0x35 } },
	{ 0x66, 1, { 0x36 } },
	{ 0x67, 1, { 0x07 } },
	{ 0x68, 1, { 0x07 } },
	{ 0x69, 1, { 0x07 } },
	{ 0x6a, 1, { 0x2e } },
	{ 0x6b, 1, { 0x2f } },
	{ 0x6c, 1, { 0x30 } },
	{ 0x6d, 1, { 0x40 } },
	{ 0x6e, 1, { 0x08 } },
	{ 0x6f, 1, { 0x12 } },
	{ 0x70, 1, { 0x10 } },
	{ 0x71, 1, { 0x13 } },
	{ 0x72, 1, { 0x11 } },
	{ 0x73, 1, { 0x2d } },
	{ 0x74, 1, { 0x01 } },
	{ 0x75, 1, { 0x07 } },
	{ 0x76, 1, { 0x07 } },
	{ 0x77, 1, { 0x28 } },
	{ 0x78, 1, { 0x41 } },
	{ 0x79, 1, { 0x07 } },
	{ 0x7a, 1, { 0x07 } },
	{ 0x7b, 1, { 0x07 } },
	{ 0x7c, 1, { 0x34 } },
	{ 0x7d, 1, { 0x35 } },
	{ 0x7e, 1, { 0x36 } },
	{ 0x7f, 1, { 0x07 } },
	{ 0x80, 1, { 0x07 } },
	{ 0x81, 1, { 0x07 } },
	{ 0x82, 1, { 0x2e } },
	{ 0x83, 1, { 0x2f } },
	{ 0x84, 1, { 0x30 } },
	{ 0x85, 1, { 0x40 } },
	{ 0x86, 1, { 0x08 } },
	{ 0x87, 1, { 0x12 } },
	{ 0x88, 1, { 0x10 } },
	{ 0x89, 1, { 0x13 } },
	{ 0x8a, 1, { 0x11 } },
	{ 0x8b, 1, { 0x2d } },
	{ 0x8c, 1, { 0x01 } },
	{ 0x8d, 1, { 0x07 } },
	{ 0x8e, 1, { 0x07 } },
	{ 0x8f, 1, { 0x28 } },
	{ 0x90, 1, { 0x41 } },
	{ 0xa2, 1, { 0x44 } },
	{ 0xa3, 1, { 0x88 } },
	{ 0xa4, 1, { 0x01 } },
	{ 0xa7, 1, { 0x50 } },
	{ 0xae, 1, { 0x0a } },
	{ 0xb2, 1, { 0x21 } },
	{ 0xb3, 1, { 0x04 } },
	{ 0xb4, 1, { 0x09 } },
	{ 0xc0, 1, { 0x07 } },
	{ 0xc1, 1, { 0x2e } },
	{ 0xc2, 1, { 0x00 } },
	{ 0xc3, 1, { 0x50 } },
	{ 0xc4, 1, { 0x00 } },
	{ 0xc5, 1, { 0x2b } },
	{ 0xc6, 1, { 0x28 } },
	{ 0xc7, 1, { 0x28 } },
	{ 0xc8, 1, { 0x00 } },
	{ 0xc9, 1, { 0x00 } },
	{ 0xca, 1, { 0x01 } },
	{ 0xd0, 1, { 0x01 } },
	{ 0xd1, 1, { 0x12 } },
	{ 0xd2, 1, { 0x00 } },
	{ 0xd3, 1, { 0x81 } },
	{ 0xd4, 1, { 0x14 } },
	{ 0xd5, 1, { 0x22 } },
	{ 0xd6, 1, { 0x41 } },
	{ 0xd7, 1, { 0x00 } },
	{ 0xd8, 1, { 0x50 } },
	{ 0xd9, 1, { 0xa5 } },
	{ 0xda, 1, { 0x00 } },
	{ 0xdb, 1, { 0x00 } },
	{ 0xdc, 1, { 0x00 } },
	{ 0xdd, 1, { 0x19 } },
	{ 0xde, 1, { 0x81 } },
	{ 0xdf, 1, { 0x60 } },
	{ 0xe0, 1, { 0x08 } },
	{ 0xe1, 1, { 0x00 } },
	{ 0xe2, 1, { 0x01 } },
	{ 0xe3, 1, { 0x00 } },
	{ 0xe4, 1, { 0xa1 } },
	{ 0xe5, 1, { 0x0b } },
	{ 0xe6, 1, { 0x22 } },
	{ 0xee, 1, { 0x01 } },
	{ 0xed, 1, { 0x55 } },
	{ 0xff, 3, { 0x78, 0x07, 0x11 } },
	{ 0x00, 1, { 0x02 } },
	{ 0x01, 1, { 0x08 } },
	{ 0x18, 1, { 0x2e } },
	{ 0x38, 1, { 0x03 } },
	{ 0x39, 1, { 0x07 } },
	{ 0x50, 1, { 0x55 } },
	{ 0xff, 3, { 0x78, 0x07, 0x02 } },
	{ 0x1b, 1, { 0x00 } },
	{ 0x01, 1, { 0x55 } },
	{ 0x02, 1, { 0x09 } },
	{ 0x24, 1, { 0x16 } },
	{ 0x46, 1, { 0x43 } },
	{ 0x47, 1, { 0x03 } },
	{ 0x5b, 1, { 0x20 } },
	{ 0x4f, 1, { 0x01 } },
	{ 0x4c, 1, { 0x0c } },
	{ 0x06, 1, { 0x69 } },
	{ 0x08, 1, { 0x00 } },
	{ 0x0e, 1, { 0x2a } },
	{ 0x0f, 1, { 0x2a } },
	{ 0x40, 1, { 0x06 } },
	{ 0x41, 1, { 0x00 } },
	{ 0x42, 1, { 0x0b } },
	{ 0x43, 1, { 0x2e } },
	{ 0x53, 1, { 0x0b } },
	{ 0x8a, 1, { 0x22 } },
	{ 0x76, 1, { 0x13 } },
	{ 0x80, 1, { 0x32 } },
	{ 0x81, 1, { 0x33 } },
	{ 0xff, 3, { 0x78, 0x07, 0x12 } },
	{ 0x48, 1, { 0x06 } },
	{ 0x49, 1, { 0x00 } },
	{ 0x4a, 1, { 0x07 } },
	{ 0x4b, 1, { 0x18 } },
	{ 0x4e, 1, { 0x08 } },
	{ 0x6c, 1, { 0x00 } },
	{ 0x52, 1, { 0x13 } },
	{ 0x53, 1, { 0x22 } },
	{ 0xc8, 1, { 0x46 } },
	{ 0xc9, 1, { 0x00 } },
	{ 0xca, 1, { 0x2a } },
	{ 0xcb, 1, { 0x2a } },
	{ 0x10, 1, { 0x06 } },
	{ 0x11, 1, { 0x00 } },
	{ 0x12, 1, { 0x07 } },
	{ 0x13, 1, { 0x11 } },
	{ 0x1e, 1, { 0x22 } },
	{ 0x16, 1, { 0x0a } },
	{ 0x17, 1, { 0x00 } },
	{ 0x1a, 1, { 0x13 } },
	{ 0x1b, 1, { 0x25 } },
	{ 0xc0, 1, { 0x34 } },
	{ 0xc1, 1, { 0x00 } },
	{ 0xc2, 1, { 0x2a } },
	{ 0xc3, 1, { 0x2a } },
	{ 0xff, 3, { 0x78, 0x07, 0x04 } },
	{ 0xbd, 1, { 0x01 } },
	{ 0xff, 3, { 0x78, 0x07, 0x05 } },
	{ 0x1b, 1, { 0x00 } },
	{ 0x1c, 1, { 0x88 } },
	{ 0x1d, 1, { 0x00 } },
	{ 0x1e, 1, { 0x88 } },
	{ 0x1f, 1, { 0x00 } },
	{ 0x20, 1, { 0x88 } },
	{ 0x61, 1, { 0xcb } },
	{ 0x6e, 1, { 0x3f } },
	{ 0x69, 1, { 0x02 } },
	{ 0x70, 1, { 0xc8 } },
	{ 0x72, 1, { 0x6a } },
	{ 0x74, 1, { 0x55 } },
	{ 0x76, 1, { 0x79 } },
	{ 0x7a, 1, { 0x3d } },
	{ 0x7b, 1, { 0x7e } },
	{ 0x7c, 1, { 0x7e } },
	{ 0xc6, 1, { 0x1b } },
	{ 0x3e, 1, { 0x50 } },
	{ 0x56, 1, { 0xf0 } },
	{ 0x46, 1, { 0x80 } },
	{ 0x47, 1, { 0x80 } },
	{ 0xb5, 1, { 0x80 } },
	{ 0xb7, 1, { 0x80 } },
	{ 0xc9, 1, { 0x90 } },
	{ 0x73, 1, { 0x1c } },
	{ 0xff, 3, { 0x78, 0x07, 0x06 } },
	{ 0x3e, 1, { 0xe2 } },
	{ 0xc0, 1, { 0x9c } },
	{ 0xc1, 1, { 0x19 } },
	{ 0xc2, 1, { 0xf0 } },
	{ 0xc3, 1, { 0x06 } },
	{ 0x13, 1, { 0x13 } },
	{ 0x12, 1, { 0xbd } },
	{ 0xff, 3, { 0x78, 0x07, 0x07 } },
	{ 0x29, 1, { 0xcf } },
	{ 0xff, 3, { 0x78, 0x07, 0x17 } },
	{ 0x20, 133, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x9c, 0x04, 0x38, 0x00, 0x0c, 0x02, 0x1c, 0x02, 0x1c, 0x02, 0x00, 0x02, 0x0e, 0x00, 0x20, 0x01, 0x1f, 0x00, 0x07, 0x00, 0x0c, 0x08, 0xbb, 0x08, 0x7a, 0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8, 0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6, 0x2a, 0xf6, 0x2b, 0x34, 0x2b, 0x74, 0x3b, 0x74, 0x6b, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ 0xff, 3, { 0x78, 0x07, 0x08 } },
	{ 0xe0, 31, { 0x00, 0x00, 0x1b, 0x46, 0x00, 0x84, 0xb0, 0xd4, 0x15, 0x0c, 0x36, 0x76, 0x25, 0xa5, 0xed, 0x28, 0x2a, 0x60, 0x9f, 0xc6, 0x3e, 0xf9, 0x1c, 0x46, 0x3f, 0x62, 0x85, 0xb0, 0x0f, 0xcb, 0xe1 } },
	{ 0xe1, 31, { 0x00, 0x00, 0x1b, 0x46, 0x00, 0x84, 0xb0, 0xd4, 0x15, 0x0c, 0x36, 0x76, 0x25, 0xa5, 0xed, 0x28, 0x2a, 0x60, 0x9f, 0xc6, 0x3e, 0xf9, 0x1c, 0x46, 0x3f, 0x62, 0x85, 0xb0, 0x0f, 0xcb, 0xe1 } },
	{ 0xff, 3, { 0x78, 0x07, 0x0b } },
	{ 0xc0, 1, { 0x84 } },
	{ 0xc1, 1, { 0x11 } },
	{ 0xc2, 1, { 0x03 } },
	{ 0xc3, 1, { 0x03 } },
	{ 0xc4, 1, { 0x65 } },
	{ 0xc5, 1, { 0x65 } },
	{ 0xd2, 1, { 0x06 } },
	{ 0xd3, 1, { 0x8c } },
	{ 0xd4, 1, { 0x05 } },
	{ 0xd5, 1, { 0x05 } },
	{ 0xd6, 1, { 0xa3 } },
	{ 0xd7, 1, { 0xa3 } },
	{ 0xc6, 1, { 0x85 } },
	{ 0xc7, 1, { 0x6c } },
	{ 0xc8, 1, { 0x04 } },
	{ 0xc9, 1, { 0x04 } },
	{ 0xca, 1, { 0x87 } },
	{ 0xcb, 1, { 0x87 } },
	{ 0xd8, 1, { 0x08 } },
	{ 0xd9, 1, { 0xbb } },
	{ 0xda, 1, { 0x06 } },
	{ 0xdb, 1, { 0x06 } },
	{ 0xdc, 1, { 0xda } },
	{ 0xdd, 1, { 0xda } },
	{ 0x94, 1, { 0x88 } },
	{ 0x95, 1, { 0x21 } },
	{ 0x96, 1, { 0x06 } },
	{ 0x97, 1, { 0x06 } },
	{ 0x98, 1, { 0xcb } },
	{ 0x99, 1, { 0xcb } },
	{ 0x9a, 1, { 0x46 } },
	{ 0x9b, 1, { 0x8c } },
	{ 0x9c, 1, { 0x05 } },
	{ 0x9d, 1, { 0x05 } },
	{ 0x9e, 1, { 0xa3 } },
	{ 0x9f, 1, { 0xa3 } },
	{ 0xaa, 1, { 0x12 } },
	{ 0xab, 1, { 0xe0 } },
	{ 0xff, 3, { 0x78, 0x07, 0x0c } },
	{ 0x00, 1, { 0x3f } },
	{ 0x01, 1, { 0xb5 } },
	{ 0x02, 1, { 0x3f } },
	{ 0x03, 1, { 0xb4 } },
	{ 0x40, 1, { 0x3f } },
	{ 0x41, 1, { 0xb5 } },
	{ 0x42, 1, { 0x3f } },
	{ 0x43, 1, { 0xb4 } },
	{ 0x80, 1, { 0x28 } },
	{ 0x81, 1, { 0xb4 } },
	{ 0x82, 1, { 0x28 } },
	{ 0x83, 1, { 0xb5 } },
	{ 0xff, 3, { 0x78, 0x07, 0x0e } },
	{ 0x00, 1, { 0xa3 } },
	{ 0x02, 1, { 0x0f } },
	{ 0x04, 1, { 0x06 } },
	{ 0x05, 1, { 0x20 } },
	{ 0x13, 1, { 0x04 } },
	{ 0xb0, 1, { 0x00 } },
	{ 0xc0, 1, { 0x12 } },
	{ 0x21, 1, { 0x28 } },
	{ 0x22, 1, { 0x04 } },
	{ 0x23, 1, { 0x28 } },
	{ 0x24, 1, { 0x84 } },
	{ 0x20, 1, { 0x03 } },
	{ 0x25, 1, { 0x11 } },
	{ 0x26, 1, { 0x65 } },
	{ 0x27, 1, { 0x20 } },
	{ 0x29, 1, { 0x67 } },
	{ 0x2d, 1, { 0x59 } },
	{ 0x30, 1, { 0x00 } },
	{ 0x2b, 1, { 0x01 } },
	{ 0x41, 1, { 0x14 } },
	{ 0x42, 1, { 0x02 } },
	{ 0x43, 1, { 0x14 } },
	{ 0x44, 1, { 0x82 } },
	{ 0x40, 1, { 0x07 } },
	{ 0x45, 1, { 0x0b } },
	{ 0x46, 1, { 0x96 } },
	{ 0x47, 1, { 0x10 } },
	{ 0x49, 1, { 0x34 } },
	{ 0xb1, 1, { 0x5f } },
	{ 0xc8, 1, { 0x5f } },
	{ 0xc9, 1, { 0x5f } },
	{ 0x4d, 1, { 0xbc } },
	{ 0x50, 1, { 0x00 } },
	{ 0x4b, 1, { 0x01 } },
	{ 0xe0, 1, { 0x06 } },
	{ 0xe1, 1, { 0x00 } },
	{ 0xe2, 1, { 0x0b } },
	{ 0xe3, 1, { 0x2e } },
	{ 0xe5, 1, { 0x0b } },
	{ 0xff, 3, { 0x78, 0x07, 0x1e } },
	{ 0xad, 1, { 0x00 } },
	{ 0xa1, 1, { 0x1f } },
	{ 0x00, 1, { 0x2d } },
	{ 0x08, 1, { 0x2d } },
	{ 0x09, 1, { 0x2d } },
	{ 0xa4, 1, { 0x00 } },
	{ 0xa5, 1, { 0x80 } },
	{ 0xa6, 1, { 0x80 } },
	{ 0xa7, 1, { 0x55 } },
	{ 0xaa, 1, { 0x00 } },
	{ 0x0a, 1, { 0x06 } },
	{ 0x0b, 1, { 0x00 } },
	{ 0x0c, 1, { 0x07 } },
	{ 0x0d, 1, { 0x11 } },
	{ 0x0e, 1, { 0x0a } },
	{ 0xbd, 1, { 0x01 } },
	{ 0xb1, 1, { 0x1f } },
	{ 0x61, 1, { 0x1e } },
	{ 0x62, 1, { 0x03 } },
	{ 0x63, 1, { 0x1e } },
	{ 0x64, 1, { 0x83 } },
	{ 0x60, 1, { 0x0b } },
	{ 0x65, 1, { 0x0b } },
	{ 0x66, 1, { 0x75 } },
	{ 0x67, 1, { 0x00 } },
	{ 0x69, 1, { 0xcd } },
	{ 0x16, 1, { 0x37 } },
	{ 0x1e, 1, { 0x37 } },
	{ 0x1f, 1, { 0x37 } },
	{ 0x6d, 1, { 0x6d } },
	{ 0x70, 1, { 0x00 } },
	{ 0x6b, 1, { 0x01 } },
	{ 0xb4, 1, { 0x00 } },
	{ 0xb5, 1, { 0x29 } },
	{ 0xb6, 1, { 0x29 } },
	{ 0xb7, 1, { 0x2a } },
	{ 0xba, 1, { 0x00 } },
	{ 0x20, 1, { 0x06 } },
	{ 0x21, 1, { 0x00 } },
	{ 0x22, 1, { 0x07 } },
	{ 0x23, 1, { 0x18 } },
	{ 0x24, 1, { 0x08 } },
	{ 0xc9, 1, { 0x02 } },
	{ 0xc0, 1, { 0x1f } },
	{ 0xc1, 1, { 0x00 } },
	{ 0xc2, 1, { 0x42 } },
	{ 0xc3, 1, { 0x42 } },
	{ 0xc4, 1, { 0x1c } },
	{ 0xc7, 1, { 0x00 } },
	{ 0xff, 3, { 0x78, 0x07, 0x00 } },
	{ 0x11, 1, { 0x00 } },
	{ REGFLAG_DELAY, 100, { } },
	{ 0xff, 3, { 0x78, 0x07, 0x06 } },
	{ 0x80, 1, { 0x00 } },
	{ 0xa3, 1, { 0x00 } },
	{ 0x4e, 1, { 0x64 } },
	{ 0x4d, 1, { 0x80 } },
	{ 0xc7, 1, { 0x05 } },
	{ 0x48, 1, { 0x09 } },
	{ 0xff, 3, { 0x78, 0x07, 0x00 } },
	{ 0x29, 1, { 0x00 } },
	{ REGFLAG_DELAY, 20, { } },
	{ 0x35, 1, { 0x00 } },
	{ 0xff, 3, { 0x78, 0x07, 0x00 } },
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[5] = {
	{ REGFLAG_DELAY, 5, { } },
	{ 0x28, 0, {  } },
	{ REGFLAG_DELAY, 20, { } },
	{ 0x10, 0, {  } },
	{ REGFLAG_DELAY, 100, { } },
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
				if (j >= 150)
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

	tran_call_tp_cmd_handle(TRAN_TP_CMD_RESUME_RESET, NULL);
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
	.clock = 344355,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1106,
	.htotal = 1128,
	.vdisplay = 2460,
	.vsync_start = 2502,
	.vsync_end = 2504,
	.vtotal = 2544,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock = 344152,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1106,
	.htotal = 1128,
	.vdisplay = 2460,
	.vsync_start = 3348,
	.vsync_end = 3350,
	.vtotal = 3390,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 344085,
	.hdisplay = 1080,
	.hsync_start = 1102,
	.hsync_end = 1106,
	.htotal = 1128,
	.vdisplay = 2460,
	.vsync_start = 5042,
	.vsync_end = 5044,
	.vtotal = 5084,
};

static struct mtk_panel_params ext_params_120hz = {
	.data_rate = 870,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
		[1] = { .cmd = 0x0d, .count = 1, .para_list = { 0x00 } },
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
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
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
	.data_rate = 870,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
		[1] = { .cmd = 0x0d, .count = 1, .para_list = { 0x00 } },
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
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
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
	.data_rate = 870,
	.esd_check_enable = 1,
	.cust_esd_check = 1,
	.lcm_esd_check_table = {
		[0] = { .cmd = 0x0a, .count = 1, .para_list = { 0x9c } },
		[1] = { .cmd = 0x0d, .count = 1, .para_list = { 0x00 } },
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
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
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
	ctx->panel.prepared = true;
	ctx->panel.enabled = true;

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
		tran_call_tp_cmd_handle(TRAN_TP_CMD_SHUTDOWN, NULL);
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
	{ .compatible = "ili7807s,fhdp,dsi,vdo,tm,tm,120hz,x6837" },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.shutdown = lcm_shutdown,
	.driver = {
		.name = "ili7807s_fhdp_dsi_vdo_tm_tm_120hz_x6837",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_DESCRIPTION("ili7807s+TM dsi vdo DSC 120hz Panel Driver");
MODULE_LICENSE("GPL v2");
