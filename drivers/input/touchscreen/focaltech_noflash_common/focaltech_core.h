/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 xtrbasement
 *
 * FocalTech SPI NoFlash TDDI touchscreen driver.
 */
#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/pm_wakeup.h>
#include <linux/jiffies.h>

#include "tran_touch_core.h"

#define FTS_DRIVER_VERSION		"FocalTech V4.1 20230424"
#define FTS_DRIVER_NAME			"mtk-tpd"

/* SPI bus definitions */
#define FTS_MAX_POINTS_SUPPORT		10
#define FTS_MAX_KEYS			4
#define FTS_KEY_DIM			10
#define FTS_ONE_TCH_LEN			6
#define FTS_TOUCH_DATA_LEN		(FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)
#define FTS_MAX_ID			0x0A

#define FTS_TOUCH_X_H_POS		3
#define FTS_TOUCH_X_L_POS		4
#define FTS_TOUCH_Y_H_POS		5
#define FTS_TOUCH_Y_L_POS		6
#define FTS_TOUCH_PRE_POS		7
#define FTS_TOUCH_AREA_POS		8
#define FTS_TOUCH_POINT_NUM		2
#define FTS_TOUCH_EVENT_POS		3
#define FTS_TOUCH_ID_POS		5

#define FTS_TOUCH_DOWN			0
#define FTS_TOUCH_UP			1
#define FTS_TOUCH_CONTACT		2

#define FTS_EVENT_DOWN(flag)		((flag) == FTS_TOUCH_DOWN || (flag) == FTS_TOUCH_CONTACT)
#define FTS_EVENT_UP(flag)		((flag) == FTS_TOUCH_UP)
#define FTS_EVENT_NO_DOWN(data)		(!(data)->point_num)

#define FTS_MAX_COMPATIBLE_TYPE		8
#define FTS_MAX_COMMMAND_LENGTH		16
#define FTS_MAX_TOUCH_BUF		4096
#define FTS_BUS_BUF_LEN			4096

/* Registers */
#define FTS_REG_WORKMODE		0x00
#define FTS_REG_WORKMODE_FACTORY_VALUE	0x40
#define FTS_REG_WORKMODE_WORK_VALUE	0x00
#define FTS_REG_POWER_MODE		0xA5
#define FTS_REG_POWER_MODE_SLEEP	0x03
#define FTS_REG_CHIP_ID			0xA3
#define FTS_REG_CHIP_ID2		0x9F
#define FTS_REG_FW_VER			0xA6
#define FTS_REG_VENDOR_ID		0xA8
#define FTS_REG_LCD_BUSY_NUM		0xAB
#define FTS_REG_FACE_DEC_MODE_EN	0xB0
#define FTS_REG_GLOVE_MODE_EN		0xC0
#define FTS_REG_COVER_MODE_EN		0xC1
#define FTS_REG_CHARGER_MODE_EN		0x8B
#define FTS_REG_EDGE_REJECT		0x8C
#define FTS_REG_GAME_MODE_EN		0xC3
#define FTS_REG_GESTURE_EN		0xD0
#define FTS_REG_GESTURE_OUTPUT_ADDRESS	0xD3
#define FTS_REG_FLOW_WORK_CNT		0x91
#define FTS_REG_ESD_LCD_ERR		0xED

#define FTS_REG_LICESS_VER		0xE4
#define FTS_REG_FW_MIN_VER		0xB2
#define FTS_REG_FW_SUB_MIN_VER		0xB3

#define FTS_SYSFS_ECHO_ON(buf)		((buf)[0] == '1')
#define FTS_SYSFS_ECHO_OFF(buf)		((buf)[0] == '0')

#define FTS_INTERVAL_READ_REG		200
#define FTS_TIMEOUT_READ_REG		1000
#define FTS_WAIT_TP_READY_RETRY		20

#define FTS_ESDCHECK_WAIT_TIME		1000
#define FTS_PRC_INTERVAL		100

/* Bus types */
enum fts_bus_type {
	BUS_TYPE_NONE = 0,
	BUS_TYPE_SPI_V2 = 1,
	BUS_TYPE_I2C = 2,
};

/* Logging macros matching stock format */
#define FTS_DEBUG(fmt, args...) \
	printk(KERN_DEBUG "[FTS_TS]%s:" fmt "\n", __func__, ##args)

#define FTS_INFO(fmt, args...) \
	printk(KERN_INFO "[FTS_TS/I]%s:" fmt "\n", __func__, ##args)

#define FTS_ERROR(fmt, args...) \
	printk(KERN_ERR "[FTS_TS/E]%s:" fmt "\n", __func__, ##args)

#define FTS_FUNC_ENTER() \
	printk(KERN_DEBUG "[FTS_TS]%s: Enter\n", __func__)

#define FTS_FUNC_EXIT() \
	printk(KERN_DEBUG "[FTS_TS]%s: Exit(%d)\n", __func__, __LINE__)

struct fts_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	bool have_key;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coords[FTS_MAX_KEYS];
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 max_touch_number;
};

struct ts_ic_info {
	bool is_incell;
	bool hid_supported;
	struct {
		u8 chip_idh;
		u8 chip_idl;
		u8 rom_idh;
		u8 rom_idl;
		u8 pb_idh;
		u8 pb_idl;
		u8 bl_idh;
		u8 bl_idl;
	} ids;
	u16 cid[FTS_MAX_COMPATIBLE_TYPE];
};

struct ts_event {
	int x;
	int y;
	int p;
	int flag;
	int id;
	int area;
	int minor;
};

struct fts_ts_data {
	struct i2c_client *client;
	struct spi_device *spi;
	struct device *dev;
	struct input_dev *input_dev;
	struct input_dev *pen_dev;
	struct fts_ts_platform_data *pdata;
	struct ts_ic_info ic_info;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct work_struct initupg_work;
	struct delayed_work esdcheck_work;
	struct delayed_work prc_work;
	struct work_struct resume_work;
	wait_queue_head_t ts_waitqueue;
	struct proc_dir_entry *proc;
	struct proc_dir_entry *proc_ta;
	spinlock_t irq_lock;
	struct mutex report_mutex;
	struct mutex bus_lock;
	unsigned long intr_jiffies;
	int irq;
	int log_level;
	int fw_is_running;
	int dummy_byte;
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
	bool power_disabled;
	bool glove_mode;
	bool cover_mode;
	bool charger_mode;
	bool gesture_support;
	bool pr_mode;
	bool prc_support;
	bool pen_support;
	bool ta_mode;
	u8 gesture_bmode;
	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
	int bus_type;
	int bus_ver;
	u8 *point_buf;
	int pnt_buf_size;
	int touch_size;
	int touchs;
	int key_state;
	int touch_point;
	int point_num;
	struct ts_event events[FTS_MAX_POINTS_SUPPORT];
	u8 *ta_buf;
	int ta_size;
	int ta_flag;
	const struct firmware *ini_fw;
	u8 fw_ver;
	u8 fw_in_tp;
};

struct upgrade_setting_nf {
	u8 rom_idh;
	u8 rom_idl;
	u16 reserved;
	u32 pb_length;
	u32 ecc_max_len;
	u8 init_cmd;
	u8 ecc_type;
	u8 delay_init;
	u8 spi_pe;
	u8 drwr_support;
	u8 ecc_delay;
	u8 half_length;
	u8 fd_check;
};

struct upgrade_module {
	u64 id;
	char vendor_name[16];
	u8 *fw_file;
	u64 fw_len;
};

struct fts_upgrade {
	struct fts_ts_data *ts_data;
	struct upgrade_module *module_info;
	struct upgrade_setting_nf *setting_nf;
	int module_id;
	u8 *fw;
	u32 fw_length;
	u8 *fw_from_request;
	u32 fw_from_request_len;
	bool fw_from_file;
};

/* Global instances */
extern struct fts_ts_data *fts_data;
extern struct tran_ts_controller fts_noflash_controller;
extern struct fts_upgrade *fwupgrade;
extern int tran_focaltech_lcd_need_reset;

/* SPI bus interface (focaltech_spi.c) */
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_bus_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen);
int fts_bus_configure(struct fts_ts_data *ts_data, u8 *buf, u32 size);

/* Core driver functions (focaltech_core.c) */
int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h);
int fts_wait_tp_to_valid(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_reset_proc(int hdelayms);
void fts_irq_disable(void);
void fts_irq_enable(void);
int fts_hid2std(void);
void fts_release_all_finger(void);
int fts_ts_suspend(struct device *dev);
int fts_ts_resume(struct device *dev);
int fts_ts_probe_entry(struct fts_ts_data *ts_data);
int fts_ts_remove_entry(struct fts_ts_data *ts_data);

/* Transsion integration (focaltech_tran.c) */
u8 fts_fwupg_get_ver_in_tp_call_transsion(void);
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_tpd_ops **ops);

/* APK debug & sysfs (focaltech_ex_fun.c) */
int fts_create_apk_debug_channel(struct fts_ts_data *ts_data);
void fts_release_apk_debug_channel(struct fts_ts_data *ts_data);
int fts_set_edge_reject_switch(u8 mode);
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);

/* Extended modes (focaltech_ex_mode.c) */
void fts_ex_mode_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);

/* Gesture (focaltech_gesture.c) */
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);

/* ESD check (focaltech_esdcheck.c) */
int idc_esdcheck_lcderror(struct fts_ts_data *ts_data);
int fts_esdcheck_proc_busy(struct fts_ts_data *ts_data, bool proc_debug);
int fts_esdcheck_switch(struct fts_ts_data *ts_data, bool enable);
int fts_esdcheck_suspend(struct fts_ts_data *ts_data);
int fts_esdcheck_resume(struct fts_ts_data *ts_data);
int fts_esd_is_disable(void);
int fts_esdcheck_init(struct fts_ts_data *ts_data);
int fts_esdcheck_exit(struct fts_ts_data *ts_data);

/* Point report check (focaltech_point_report_check.c) */
void fts_prc_queue_work(struct fts_ts_data *ts_data);
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);

/* Firmware download / noflash (focaltech_flash.c) */
int fts_upgrade_bin(char *fw_name, bool force);
int fts_enter_test_environment(bool test_state);
int fts_fw_recovery(void);
int fts_enter_gesture_fw(void);
int fts_enter_normal_fw(void);
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);

/* Factory self-test (focaltech_test.c) */
int fts_test_init(struct fts_ts_data *ts_data);
int fts_test_exit(struct fts_ts_data *ts_data);
int tran_fts_get_SelftestResult(void);

#endif /* __LINUX_FOCALTECH_CORE_H__ */
