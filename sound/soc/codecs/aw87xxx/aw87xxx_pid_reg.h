/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_PID_REG_H__
#define __AW87XXX_PID_REG_H__

#include <linux/types.h>

#define AW87XXX_REG_CHIPID		0x00

#define REG_NONE_ACCESS			0x00
#define REG_RD_ACCESS			(1 << 0)
#define REG_WR_ACCESS			(1 << 1)
#define REG_RDWR_ACCESS			(REG_RD_ACCESS | REG_WR_ACCESS)

/* Product IDs */
#define AW_DEV_CHIPID_18		0x18
#define AW_DEV_CHIPID_39		0x39
#define AW_DEV_CHIPID_59		0x59
#define AW_DEV_CHIPID_5A		0x5A
#define AW_DEV_CHIPID_76		0x76
#define AW_DEV_CHIPID_9A		0x9A
#define AW_DEV_CHIPID_9B		0x9B

/* Soft reset default register / value */
#define AW_PID_SOFTRST_REG		0x00
#define AW_PID_SOFTRST_VAL		0xAA

/* PID 9B (AW87319) specific definitions */
#define AW_PID_9B_REG_MAX		0x64
#define AW_PID_9B_ENCRYPTION_REG	0x64
#define AW_PID_9B_ENCRYPTION_BOOST	0x2C
#define AW_PID_9B_SYSCTRL_REG		0x01
#define AW_PID_9B_SPK_MODE_ENABLE	0x04
#define AW_PID_9B_SPK_MODE_DISABLE	0x00
#define AW_PID_9B_SPK_MODE_MASK		0xFFFB
#define AW_PID_9B_ESD_REG		0x01
#define AW_PID_9B_ESD_CHECK_VAL		0x03
#define AW_PID_9B_REC_REG		0x01
#define AW_PID_9B_REC_ENABLE		0x00
#define AW_PID_9B_REC_DISABLE		0x01
#define AW_PID_9B_REC_MASK		0xFE

/* PID 39 (AW87329 / AW87339 / AW87349) specific definitions */
#define AW_PID_39_REG_MAX		0x65
#define AW_PID_39_ESD_REG		0x02
#define AW_PID_39_ESD_CHECK_VAL		0xA0
#define AW_PID_39_REC_REG		0x02
#define AW_PID_39_REC_ENABLE		0x01
#define AW_PID_39_REC_DISABLE		0x00
#define AW_PID_39_REC_MASK		0xF7

/* PID 59_5X9 (AW87509 / AW87519 / AW87529 / AW87539) specific definitions */
#define AW_PID_59_5X9_REG_MAX		0x70
#define AW_PID_59_5X9_ESD_REG		0x69
#define AW_PID_59_5X9_ESD_CHECK_VAL	0x00
#define AW_PID_59_5X9_REC_REG		0x01
#define AW_PID_59_5X9_REC_ENABLE	0x01
#define AW_PID_59_5X9_REC_DISABLE	0x00
#define AW_PID_59_5X9_REC_MASK		0xF7

/* PID 59_3X9 (AW87359 / AW87389) specific definitions */
#define AW_PID_59_3X9_REG_MAX		0x71
#define AW_PID_59_3X9_ESD_REG		0x70
#define AW_PID_59_3X9_ESD_CHECK_VAL	0x00
#define AW_PID_59_3X9_REC_REG		0x02
#define AW_PID_59_3X9_REC_ENABLE	0x00
#define AW_PID_59_3X9_REC_DISABLE	0x01
#define AW_PID_59_3X9_REC_MASK		0xFB

/* PID 5A (AW87549 / AW87559 / AW87569 / AW87579 / AW81509) specific definitions */
#define AW_PID_5A_REG_MAX		0x78
#define AW_PID_5A_ESD_REG		0x62
#define AW_PID_5A_ESD_CHECK_VAL		0x02
#define AW_PID_5A_REC_REG		0x01
#define AW_PID_5A_REC_ENABLE		0x01
#define AW_PID_5A_REC_DISABLE		0x00
#define AW_PID_5A_REC_MASK		0xFB

/* PID 76 (AW87390 / AW87320 / AW87401) specific definitions */
#define AW_PID_76_REG_MAX		0x79
#define AW_PID_76_ESD_REG		0x67
#define AW_PID_76_ESD_CHECK_VAL		0x04
#define AW_PID_76_REC_REG		0x02
#define AW_PID_76_REC_ENABLE		0x00
#define AW_PID_76_REC_DISABLE		0x01
#define AW_PID_76_REC_MASK		0xFB

/* PID 18 table definitions */
#define AW_PID_18_REG_MAX		0x67

#endif /* __AW87XXX_PID_REG_H__ */
