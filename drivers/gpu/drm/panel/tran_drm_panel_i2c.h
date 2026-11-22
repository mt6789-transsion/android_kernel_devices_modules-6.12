/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _TRAN_DRM_PANEL_I2C_H_
#define _TRAN_DRM_PANEL_I2C_H_

int tran_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
int tran_panel_i2c_read_bytes(unsigned char addr, unsigned char *value);

#endif /* _TRAN_DRM_PANEL_I2C_H_ */
