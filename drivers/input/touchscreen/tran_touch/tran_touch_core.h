/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _TRAN_TOUCH_CORE_H_
#define _TRAN_TOUCH_CORE_H_

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#define TRAN_LOG_TAG "[tran_touch-INFO]"
#define TRAN_ERR_TAG "[tran_touch-ERROR]"

#define tran_info(fmt, args...) \
	printk(KERN_INFO "[tran_touch-INFO]%s %d:" fmt, __func__, __LINE__, ##args)

#define tran_err(fmt, args...) \
	printk(KERN_ERR "[tran_touch-ERROR]%s %d:" fmt, __func__, __LINE__, ##args)

#define tran_func_in() \
	printk(KERN_INFO "[tran_touch]++++++++++%s: In(%d)++++++++++\n", __func__, __LINE__)

#define tran_func_out() \
	printk(KERN_INFO "[tran_touch]----------%s: Exit(%d)----------\n", __func__, __LINE__)

/* Boot modes returned by platform->ops->get_boot_mode() */
enum tran_boot_mode {
	TRAN_NORMAL_BOOT = 0,
	TRAN_NOT_RUN_MODE = 1,
	TRAN_FACTORY_BOOT = 2,
};

/* Core touch power/suspend states */
enum tran_ts_state {
	TRAN_ACTIVE_MODE = 10,
	TRAN_SUSPEND_MODE = 11,
	TRAN_PS_SUSPEND_MODE = 12,
	TRAN_GESTURE_SUSPEND_MODE = 13,
};

/* State machine triggers */
enum tran_ts_trigger {
	TRAN_RESUME_TRIGGER = 0,
	TRAN_SUSPEND_TRIGGER = 1,
};

/* Feature enable bitmask (tran_ts_core.func_enable) */
#define TRAN_FUNC_PSENSOR_BIT		BIT(0)
#define TRAN_FUNC_GESTURE_BIT		BIT(1)

/* Proximity sensor states/events */
#define TRAN_PS_STATE_IDLE		2000
#define TRAN_PS_STATE_NEAR		2001
#define TRAN_PS_STATE_FAR		2002
#define TRAN_PS_EVENT_DEFAULT		2004

/* Controller set_ts_mode() command IDs */
enum tran_ts_mode_cmd {
	TRAN_MODE_CHARGER = 100,
	TRAN_MODE_PROXIMITY = 101,
	TRAN_MODE_GAME_STATE = 102,
	TRAN_MODE_EDGE_REJECT = 103,
	TRAN_MODE_GAME_MODE = 104,
	TRAN_MODE_REPORT_RATE = 105,
	TRAN_MODE_TRANSIENT_REPORT_RATE = 106,
};

/* External command IDs for tran_call_tp_cmd_handle() */
enum tran_tp_ext_cmd {
	TRAN_TP_CMD_RESET_LEVEL = 1001,
	TRAN_TP_CMD_RESUME_BY_DDI = 1002,
	TRAN_TP_CMD_GET_GESTURE_ENABLE = 1003,
};

/* Gesture category types */
enum tran_gesture_type {
	TRAN_GESTURE_DOUBLE = 0,
	TRAN_GESTURE_MUSIC = 1,
	TRAN_GESTURE_CHAR = 2,
	TRAN_GESTURE_CHAR_ALL = 3,
	TRAN_GESTURE_CLICK = 4,
};

/* MTK display notifier events/values */
#define MTK_DISP_EARLY_EVENT_BLANK	0x00
#define MTK_DISP_EVENT_BLANK		0x01
#define MTK_DISP_BLANK_UNBLANK		0x00
#define MTK_DISP_BLANK_POWERDOWN	0x01
#define MTK_DISP_BLANK_UNBLANK_AFTER	0x02

struct tran_ts_core;

/*
 * Per-chip gesture ID translation entry (size = 2 bytes).
 * Arrays provided by controllers are terminated with { 0, 0 } (tran_id == 0).
 */
struct tran_gesture_chip_map {
	u8 chip_id;
	u8 tran_id;
};

/*
 * Core gesture mapping table entry (size = 12 bytes).
 */
struct tran_gesture_map {
	char name[2];
	u8 reserved;
	u8 enabled;
	u8 code;
	u8 pad[3];
	int type;
};

/*
 * Touch controller primary operations (size = 0x58 / 88 bytes).
 * Exact function pointer signatures verified against stock Clang CFI hashes.
 */
struct tran_ts_controller_ops {
	int (*init)(void);
	void (*suspend)(struct device *dev);
	void (*resume)(struct device *dev);
	void (*set_ts_mode)(unsigned int mode, unsigned int val);
	u8 (*get_firmware_version)(void);
	u8 (*get_base_version)(void);
	u8 (*get_cfg_version)(void);
	int (*tp_selftest)(void);
	int (*get_selftest_data)(char *buf);
	int (*tran_selftest_open)(struct inode *inode, struct file *file);
	void *reserved;
};

/*
 * Touch controller extended operations for display DDI callbacks (size = 0x10 / 16 bytes).
 */
struct tran_ts_controller_ext_ops {
	void (*tp_reset_level)(int level);
	void (*resume_by_ddi)(void);
};

/*
 * Touch controller descriptor registered by FocalTech / Ilitek drivers (size = 0x48 / 72 bytes).
 * Struct tag MUST be `struct tran_ts_controller` to match CFI _ZTSFiP18tran_ts_controllerE.
 */
struct tran_ts_controller {
	const char *ic_name;
	const char *vendor;
	const struct tran_gesture_chip_map *gesture_maps;
	char *selftest_data;
	const char **support_list;
	struct input_dev *input_dev;
	struct tran_ts_controller_ops *ops;
	struct tran_ts_controller_ext_ops *ext_ops;
	struct tran_ts_core *ts;
};

/*
 * Core callbacks exposed to touch controllers via controller->ts->tpd_ops (size = 0x30 / 48 bytes).
 */
struct tran_tpd_ops {
	int (*get_gesture_status)(void);
	int (*gesture_switch_status)(int type);
	int (*gesture_id_translate)(int raw_id);
	int (*get_proximity_state)(unsigned int *state);
	int (*report_proximity_event)(unsigned int event);
	void (*controller_init_finish)(void);
};

/*
 * DT supplier descriptor parsed from supplier-0..N properties (size = 0x28 / 40 bytes).
 */
struct tran_supplier_info {
	int id;
	int resolution;
	const char *name;
	const char *vendor;
	struct list_head list;
};

/*
 * Registered controller list wrapper (size = 0x18 / 24 bytes).
 */
struct tran_controller_node {
	struct tran_ts_controller *controller;
	struct list_head list;
};

/*
 * Platform operations table (size = 0x20 / 32 bytes).
 */
struct tran_platform_ops {
	int (*plat_init)(struct tran_ts_core *ts);
	int (*plat_deinit)(struct tran_ts_core *ts);
	int (*find_lcm_name)(char *lcm_name);
	int (*get_boot_mode)(void);
};

/*
 * Platform descriptor (size = 0x18 / 24 bytes).
 */
struct tran_platform_info {
	const char *selftest_name;
	struct tran_platform_ops *ops;
	void *private_data;
};

/*
 * Proximity state data embedded at tran_ts_core + 0x10 (size = 0x0c / 12 bytes).
 */
struct tran_ps_data {
	unsigned int state;
	unsigned int reserved;
	unsigned int event;
};

/*
 * Main Transsion touch core state structure (size = 0x1f0 / 496 bytes).
 * Struct tag MUST be `struct tran_ts_core` to match CFI _ZTSFiP12tran_ts_coreE.
 * Field offsets are part of the binary ABI shared with focaltech & ilitek modules.
 */
struct tran_ts_core {
	int edge_reject_switch;				/* +0x000 */
	int boot_mode;					/* +0x004 */
	u8 usb_in;					/* +0x008 */
	u8 probe_done;					/* +0x009 */
	u8 pad_0a[2];					/* +0x00a */
	int state;					/* +0x00c */
	struct tran_ps_data ps_data;			/* +0x010 */
	int fod_down;					/* +0x01c */
	u8 game_state;					/* +0x020 */
	u8 transient_report_rate;			/* +0x021 */
	u8 report_rate;					/* +0x022 */
	u8 game_mode;					/* +0x023 */
	u32 func_enable;				/* +0x024 */
	struct platform_device *pdev;			/* +0x028 */
	struct mutex dsp_lock;				/* +0x030 */
	struct mutex list_lock;				/* +0x060 */
	struct completion init_done;			/* +0x090 */
	struct workqueue_struct *init_wq;		/* +0x0b0 */
	struct workqueue_struct *work_wq;		/* +0x0b8 */
	struct work_struct init_work;			/* +0x0c0 */
	struct work_struct dsp_work;			/* +0x0f0 */
	struct work_struct usb_work;			/* +0x120 */
	struct work_struct match_work;			/* +0x150 */
	struct notifier_block disp_notifier;		/* +0x180 */
	struct notifier_block psy_notifier;		/* +0x198 */
	struct list_head supplier_list;			/* +0x1b0 */
	struct list_head controller_list;		/* +0x1c0 */
	struct tran_tpd_ops *tpd_ops;			/* +0x1d0 */
	struct tran_supplier_info *supplier;		/* +0x1d8 */
	struct tran_ts_controller *controller;		/* +0x1e0 */
	struct tran_platform_info *platform;		/* +0x1e8 */
};

/* Global instances */
extern struct tran_ts_core g_tran_ts;
extern struct tran_tpd_ops g_tpd_ops;
extern struct tran_ps_data *g_ps_data;
extern struct tran_gesture_map gesture_maps[17];

/* Exported kernel module ABI */
int register_ts_controller(struct tran_ts_controller *controller);
int unregister_ts_controller(struct tran_ts_controller *controller);
unsigned int tran_call_tp_cmd_handle(unsigned int cmd, void *data);

/* Internal cross-file functions */
int state_change_machine(int trigger, int cur_state);
void gesture_init(struct tran_ts_core *ts);
void gesture_deinit_node(void);
void gesture_clear_data(void);
int is_all_dead(u8 *data, int len);
void tran_node_init(struct tran_ts_core *ts);
void init_tran_platform_info(void);
int mtk_node_create(struct tran_ts_core *ts);
int mtk_register_charger_notify(struct tran_ts_core *ts);
int mtk_unregister_charger_notify(void);
void proximity_init(struct tran_ts_core *ts);

/* External display notifier symbols from mtk_disp_notify.ko */
extern int mtk_disp_notifier_register(const char *source, struct notifier_block *nb);
extern int mtk_disp_notifier_unregister(struct notifier_block *nb);

#endif /* _TRAN_TOUCH_CORE_H_ */
