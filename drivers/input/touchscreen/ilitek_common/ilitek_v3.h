/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 xtrbasement
 *
 * ILITEK Touch IC driver (v3.0.9.2).
 */
#ifndef __ILITEK_V3_H
#define __ILITEK_V3_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/pm_wakeup.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#include "tran_touch_core.h"

#define DRIVER_VERSION			"3.0.9.2.230412"

/* Logging macros matching stock format */
extern bool debug_en;

#define ILI_INFO(fmt, arg...) \
	printk(KERN_INFO "ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg)

#define ILI_ERR(fmt, arg...) \
	printk(KERN_ERR "ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg)

#define ILI_DBG(fmt, arg...) do { \
	if (debug_en) \
		printk(KERN_INFO "ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg); \
} while (0)

#define ENABLE				1
#define DISABLE				0

/* Bus & platform defaults (BUS_SPI / BUS_I2C come from linux/input.h) */
#define EMP_FW_PROC			2001

#define SPI_WRITE			0x82
#define SPI_READ			0x83
#define SPI_ACK				0xA3
#define SPI_DUMMY			0xFF
#define SPI_TX_BUF_SIZE			4096
#define SPI_RX_BUF_SIZE			4096
#define SPI_TRANSFER_MAX_LEN		0x1800

/* Touch geometry defaults */
#define TOUCH_SCREEN_X_MIN		0
#define TOUCH_SCREEN_Y_MIN		0
#define TOUCH_SCREEN_X_MAX		1080
#define TOUCH_SCREEN_Y_MAX		2460
#define MAX_TOUCH_NUM			10
#define MAX_PEN_NUM			1
#define TPD_WIDTH			2048

/* Protocol versions */
#define PROTOCOL_VER_500		0x050000
#define PROTOCOL_VER_510		0x050100
#define PROTOCOL_VER_520		0x050200
#define PROTOCOL_VER_530		0x050300
#define PROTOCOL_VER_540		0x050400
#define PROTOCOL_VER_550		0x050500
#define PROTOCOL_VER_560		0x050600
#define PROTOCOL_VER_570		0x050700

/* Protocol commands */
#define P5_X_READ_DATA_CTRL		0xF6
#define P5_X_GET_TP_INFORMATION		0x20
#define P5_X_GET_KEY_INFORMATION	0x27
#define P5_X_GET_PANEL_INFORMATION	0x29
#define P5_X_GET_FW_VERSION		0x21
#define P5_X_GET_PROTOCOL_VERSION	0x22
#define P5_X_GET_CORE_VERSION		0x23
#define P5_X_GET_CORE_VERSION_NEW	0x24
#define P5_X_GET_REPORT_INFORMATION	0x37
#define P5_X_MODE_CONTROL		0xF0
#define P5_X_SET_CDC_INIT		0xF1
#define P5_X_GET_CDC_DATA		0xF2
#define P5_X_CDC_BUSY_STATE		0xF3
#define P5_X_MP_TEST_MODE_INFO		0xFE
#define P5_X_I2C_UART			0x40
#define CMD_GET_TP_SIGNAL_INFORMATION	0x17
#define CMD_GET_TP_USB_DETECT		0x18
#define CMD_GET_PANEL_INFO		0x2F

/* Packet IDs */
#define P5_X_DEMO_PACKET_ID		0x5A
#define P5_X_DEMO_PACKET_ID_HIGH_RES	0x5B
#define P5_X_DEBUG_PACKET_ID		0xA7
#define P5_X_DEMO_DEBUG_INFO_PACKET_ID	0x5C
#define P5_X_TEST_PACKET_ID		0xF2
#define P5_X_GESTURE_PACKET_ID		0xAA
#define P5_X_GESTURE_FAIL_ID		0xAE
#define P5_X_I2CUART_PACKET_ID		0x7A
#define P5_X_DEMO_MODE_PACKET_LEN	43
#define P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION 72
#define P5_X_INFO_HEADER_LENGTH		3
#define P5_X_INFO_CHECKSUM_LENGTH	1
#define P5_X_DEMO_DEBUG_INFO_ID0_LENGTH	14
#define P5_X_DEBUG_MODE_PACKET_LENGTH	1280
#define P5_X_TEST_MODE_PACKET_LENGTH	1180
#define P5_X_GESTURE_NORMAL_LENGTH	8
#define P5_X_GESTURE_INFO_LENGTH	170
#define P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION 221
#define P5_X_PROXIMITY_PACKET_ID	0xAA

/* FW modes */
#define P5_X_FW_AP_MODE			0x00
#define P5_X_FW_TEST_MODE		0x01
#define P5_X_FW_GESTURE_MODE		0x0F
#define P5_X_FW_DELTA_DATA_MODE		0x03
#define P5_X_FW_RAW_DATA_MODE		0x08

/* Data format modes */
#define DATA_FORMAT_DEMO		0x00
#define DATA_FORMAT_DEBUG		0x01
#define DATA_FORMAT_DEMO_DEBUG_INFO	0x04
#define DATA_FORMAT_DEBUG_LITE_ROI	0x05
#define DATA_FORMAT_DEBUG_LITE_WINDOW	0x06
#define DATA_FORMAT_DEBUG_LITE_AREA	0x07
#define DATA_FORMAT_GESTURE_NORMAL	0x01
#define DATA_FORMAT_GESTURE_INFO	0x02
#define DATA_FORMAT_GESTURE_DEMO	0x03

/* ICE registers */
#define ICE_HEADER_REG			0x181062
#define PID_ADDR			0x04009C
#define OTP_ID_ADDR			0x0400A0
#define ANA_ID_ADDR			0x0400A4
#define WDT_ADDR			0x05100C
#define PC_COUNTER_ADDR			0x044008
#define PC_LATCH_ADDR			0x051010
#define CHIP_RESET_ADDR			0x040050
#define CHIP_RESET_KEY			0x00017788
#define DMA_CRC_ADDR			0x04101C
#define DMA1_BASE_ADDR			0x072100

/* Chip IDs */
#define ILI9881_CHIP			0x9881
#define ILI7807_CHIP			0x7807
#define ILI9881N_AA			0x98811700
#define ILI9881O_AA			0x98811800
#define ILI9882_CHIP			0x9882
#define ILI7807S_CHIP			0x7807
#define ILI77600A_CHIP			0x7760

/* Reset methods */
enum TP_RST_METHOD {
	TP_IC_WHOLE_RST = 0,
	TP_IC_CODE_RST,
	TP_HW_RST_ONLY,
};

/* Sleep control */
enum TP_SLEEP_STATUS {
	TP_SUSPEND = 0,
	TP_DEEP_SLEEP = 1,
	TP_RESUME = 2,
};

/* Check busy modes */
enum TP_CHECK_BUSY {
	AP_MODE = 0,
	MP_MODE = 1,
};

/* WQ ctrl types */
enum TP_WQ_TYPE {
	WQ_ESD = 0,
	WQ_BAT = 1,
};

/* ICE mode */
enum ICE_MODE_CTRL {
	BOTH = 0,
	MASTER = 1,
	SLAVE = 2,
};

/* Gesture codes */
#define GESTURE_DOUBLECLICK		0x58
#define GESTURE_UP			0x60
#define GESTURE_DOWN			0x61
#define GESTURE_LEFT			0x62
#define GESTURE_RIGHT			0x63
#define GESTURE_M			0x64
#define GESTURE_W			0x65
#define GESTURE_C			0x66
#define GESTURE_E			0x67
#define GESTURE_V			0x68
#define GESTURE_O			0x69
#define GESTURE_S			0x6A
#define GESTURE_Z			0x6B

/* Debug buffer */
#define TR_BUF_SIZE			(2 * 1024)
#define TR_BUF_LIST_SIZE		(1 + (1024))
#define DEBUG_DATA_FILE_SIZE		(10 * 1024)
#define DEBUG_DATA_FILE_PATH		"/sdcard/ILITEK_log.csv"
#define CSV_LCM_ON_PATH			"/sdcard/ilitek_mp_lcm_on_log"
#define CSV_LCM_OFF_PATH		"/sdcard/ilitek_mp_lcm_off_log"
#define POWER_STATUS_PATH		"/sys/class/power_supply/battery/status"
#define DUMP_FLASH_PATH			"/sdcard/flash_dump"
#define DUMP_IRAM_PATH			"/sdcard/iram_dump"
#define INI_NAME_PATH			"/sdcard/mp.ini"
#define UPDATE_FW_PATH			"/sdcard/ILITEK_FW"
#define MAX_HEX_FILE_SIZE		(256 * 1024)
#define MAX_FLASH_FIRMWARE_SIZE		(256 * 1024)
#define MAX_IRAM_FIRMWARE_SIZE		(60 * 1024)
#define ILI_FILE_HEADER			256
#define MAX_BLOCK_NUM			20

/* IOCTL commands */
#define ILITEK_IOCTL_MAGIC		100
#define ILITEK_IOCTL_MAXNR		27

struct ilitek_hwif_info {
	u8 bus_type;
	u8 plat_type;
	u32 chip_id;
	const char *name;
	struct module *owner;
	const struct of_device_id *of_match_table;
	int (*plat_probe)(void);
	int (*plat_remove)(void);
};

struct ilitek_protocol_info {
	u32 ver;
	int fw_ver_len;
	int pro_ver_len;
	int tp_info_len;
	int key_info_len;
	int panel_info_len;
	int core_ver_len;
	int func_ctrl_len;
	int window_len;
	int cdc_len;
	int mp_info_len;
};

struct ilitek_ic_func_ctrl {
	const char *name;
	u8 cmd[8];
	int len;
	u8 def_cmd;
	u8 rec_state;
	u8 rec_cmd;
};

struct ilitek_ic_info {
	u8 type;
	u8 ver;
	u16 id;
	u32 pid;
	u32 pid_addr;
	u32 pc_counter_addr;
	u32 pc_latch_addr;
	u32 reset_addr;
	u32 otp_addr;
	u32 ana_addr;
	u32 otp_id;
	u32 ana_id;
	u32 fw_ver;
	u32 core_ver;
	u32 fw_mp_ver;
	u32 max_count;
	u32 reset_key;
	u16 wtd_key;
	int no_bk_shift;
	bool dma_reset;
	int (*dma_crc)(u32 start_addr, u32 block_size);
};

struct ilitek_touch_info {
	u16 id;
	u16 x;
	u16 y;
	u16 pressure;
};

struct ilitek_axis_info {
	s8 degree;
	u16 width_major;
	u16 width_minor;
};

struct ilitek_pen_info {
	u16 x;
	u16 y;
	u16 pressure;
	s32 tilt_x;
	s32 tilt_y;
	u8 pen_id[8];
	u8 distance_cnt;
	bool tip_sw;
	bool ring_sw;
	bool btn1;
	bool btn2;
	u8 battery;
	u8 mode;
	bool active;
};

struct gesture_symbol {
	u8 double_tap                 : 1;
	u8 alphabet_line_2_top        : 1;
	u8 alphabet_line_2_bottom     : 1;
	u8 alphabet_line_2_left       : 1;
	u8 alphabet_line_2_right      : 1;
	u8 alphabet_m                 : 1;
	u8 alphabet_w                 : 1;
	u8 alphabet_c                 : 1;
	u8 alphabet_E                 : 1;
	u8 alphabet_V                 : 1;
	u8 alphabet_O                 : 1;
	u8 alphabet_S                 : 1;
	u8 alphabet_Z                 : 1;
	u8 alphabet_V_down            : 1;
	u8 alphabet_V_left            : 1;
	u8 alphabet_V_right           : 1;
	u8 alphabet_two_line_2_bottom : 1;
	u8 alphabet_F                 : 1;
	u8 alphabet_AT                : 1;
	u8 reserve0                   : 5;
};

struct report_info_block {
	u8 nReportByPixel     : 1;
	u8 nIsHostDownload    : 1;
	u8 nIsSPIICE          : 1;
	u8 nIsSPISLAVE        : 1;
	u8 nIsI2C             : 1;
	u8 nReserved00        : 3;
	u8 nReportResolutionMode : 3;
	u8 nCustomerType      : 5;
	u8 nReserved02        : 8;
	u8 nReserved03        : 8;
};

struct debug_buf_list {
	bool mark;
	unsigned char *data;
};

struct ilitek_ts_data {
	struct i2c_client *i2c;
	struct spi_device *spi;
	struct input_dev *input;
	struct input_dev *input_pen;
	struct device *dev;
	struct wakeup_source *ws;

	struct ilitek_hwif_info *hwif;
	struct ilitek_ic_info *chip;
	struct ilitek_protocol_info *protocol;
	struct gesture_symbol ges_sym;
	struct report_info_block rib;

	struct mutex touch_mutex;
	struct mutex debug_mutex;
	struct mutex debug_read_mutex;
	spinlock_t irq_spin;

	struct completion pm_completion;
	bool pm_suspend;

	/* Physical panel parameters */
	u16 max_x;
	u16 max_y;
	u16 min_x;
	u16 min_y;
	u16 panel_wid;
	u16 panel_hei;
	u8 xch_num;
	u8 ych_num;
	u8 stx;
	u8 srx;
	u8 *update_buf;
	u8 *tr_buf;
	u8 *spi_tx;
	u8 *spi_rx;
	struct firmware tp_fw;
	const struct firmware *ini_fw;

	int actual_tp_mode;
	int tp_data_mode;
	int tp_data_format;
	int tp_data_len;
	int tp_data_shift;

	int irq_num;
	int irq_gpio;
	int irq_tirgger_type;
	int tp_rst;
	int tp_int;
	int wait_int_timeout;

	int finger;
	int curt_touch[MAX_TOUCH_NUM];
	int prev_touch[MAX_TOUCH_NUM];
	int last_touch;

	int fw_retry;
	int fw_update_stat;
	int fw_open;
	u8 fw_info[75];
	u8 fw_mp_ver[4];
	u8 edge_palm_para[32];
	u32 fw_base_ver;
	bool wq_ctrl;
	bool wq_esd_ctrl;
	bool wq_bat_ctrl;

	u16 addr_bi;
	bool netlink;
	bool report;
	bool gesture;
	bool mp_retry;
	bool knuckle;
	int gesture_mode;
	int gesture_demo_ctrl;
	u16 ges_coord[64];

	/* Debug node */
	bool dnp;
	int dbf;
	int odi;
	bool all_frame_full;
	int dbf_in_idx;
	int dbf_out_idx;
	int ppb;
	wait_queue_head_t inq;
	struct debug_buf_list *dbl;
	int raw_count;
	int delta_count;
	int bg_count;

	/* Status flags */
	int reset;
	int rst_edge_delay;
	int fw_upgrade_mode;
	int mp_ret_len;
	bool wtd_ctrl;
	bool force_fw_update;
	bool irq_after_recovery;
	bool ddi_rest_done;
	bool resume_by_ddi;
	bool tp_suspend;
	bool info_from_hex;
	bool prox_near;
	bool gesture_load_code;
	bool trans_xy;
	bool ss_ctrl;
	bool node_update;
	bool int_pulse;
	bool pll_clk_wakeup;
	bool power_status;
	bool prox_face_mode;
	bool eng_flow;
	bool ice_flash_cs_ctrl;
	bool cascade_ctrl_mode;
	bool slave_wr_ctrl;
	bool pen_info_block;
	bool compress_disable;
	bool compress_handonly_disable;
	bool compress_penonly_disable;

	/* Pen */
	struct ilitek_pen_info pen_info;
	int pen_data_len;
	u8 pen_mode;

	/* Transsion paths */
	char fw_name[128];
	char ini_name[128];

	/* Atomic state */
	atomic_t irq_stat;
	atomic_t tp_reset;
	atomic_t ice_stat;
	atomic_t fw_stat;
	atomic_t mp_stat;
	atomic_t tp_sleep;
	atomic_t tp_sw_mode;
	atomic_t cmd_int_check;
	atomic_t esd_stat;
	atomic_t ignore_report;

	/* Work / thread */
	struct task_struct *update_thread;
	struct delayed_work esd_work;
	struct delayed_work bat_work;

	/* Function pointers (matching stock CFI signatures!) */
	int (*wrapper)(u8 *wdata, u32 wlen, u8 *rdata, u32 rlen, bool spi_irq, bool i2c_irq);
	int (*detect_int_stat)(bool status);
	int (*int_mode)(bool mode);
	int (*spi_write_then_read)(struct spi_device *spi, const void *txbuf,
				   unsigned int n_tx, void *rxbuf, unsigned int n_rx);
	int (*ice_mode_ctrl)(bool enable, bool mcu, int mode);
	int (*ges_recover)(void);
	int (*esd_recover)(void);
	int (*gesture_move_code)(int mode);
	int (*mp_move_code)(void);
	void (*demo_debug_info[5])(u8 *buf, size_t len);
	int (*esd_func)(void);
};

/* Global variables */
extern struct ilitek_ts_data *ilits;
extern struct tran_ts_controller ili7807_controller;
extern struct workqueue_struct *ini_wq;
extern struct delayed_work ini_work;
extern struct ilitek_ic_func_ctrl func_ctrl[];

/* ilitek_v3_tran.c */
u8 ili_fwupg_get_ver_in_tp_call_transsion(void);
int get_tpd_ops(struct tran_ts_controller *controller, struct tran_tpd_ops **ops);

/* ilitek_v3.c */
void ili_resume_by_ddi(void);
int ili_reset_ctrl(int mode);
int ili_cascade_reset_ctrl(int reset_mode, bool enter_ice);
int ili_set_tp_data_len(int format, bool send, u8 *data);
int ili_set_pen_data_len(u8 header, u8 ctrl, u8 type);
int ili_switch_tp_mode(u8 mode);
int ili_fw_upgrade_handler(void *data);
void ili_wq_ctrl(int type, int ctrl);
int ili_mp_test_handler(char *apk, bool lcm_on);
int ili_gesture_recovery(void);
int ili_spi_recovery(void);
int ili_wq_esd_spi_check(void);
int ili_wq_esd_i2c_check(void);
int ili_sleep_handler(int mode);
void ili_report_handler(void);
int ili_tddi_init(void);
int ili_dev_init(struct ilitek_hwif_info *hwif);
void ili_dev_remove(bool flag);

/* ilitek_v3_spi.c */
int ili_spi_write_then_read_split(struct spi_device *spi, const void *txbuf,
				  unsigned int n_tx, void *rxbuf, unsigned int n_rx);
int ili_core_spi_setup(int num);
int ili_interface_dev_init(struct ilitek_hwif_info *hwif);
void ili_interface_dev_exit(struct ilitek_ts_data *ts, bool flag);

/* ilitek_v3_mtk.c */
void ili_tp_reset(void);
void ili_input_register(void);
void ili_input_pen_register(void);
void ili_irq_disable(void);
void ili_irq_enable(void);
void ili_irq_unregister(void);
int ili_irq_register(int type);
void ili_ts_suspend(struct device *dev);
void ili_ts_resume(struct device *dev);
void ilitek_reset_output(int level);
int ilitek_commmon_init(void);

/* ilitek_v3_ic.c */
int ili_ic_func_ctrl_export(const char *name, int ctrl);
int ili_tddi_ic_sram_test(void);
int ili_ice_mode_write(u32 addr, u32 data, int len);
int ili_ice_mode_read(u32 addr, u32 *data, int len);
int ili_ice_mode_write_by_mode(u32 addr, u32 data, int len, int mode);
int ili_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value);
void ili_cascade_sync_ctrl(bool mode);
int ili_ice_mode_ctrl(bool enable, bool mcu);
int ili_ice_mode_ctrl_by_mode_spi(bool enable, bool mcu, int mode);
int ili_set_bypass_mode(bool mcu);
int ili_ice_mode_ctrl_by_mode_i2c(bool enable, bool mcu, int mode);
int ili_ice_mode_ctrl_by_mode(bool enable, bool mcu, int mode);
int ili_ice_mode_read_by_mode(u32 addr, u32 *data, int len, int mode);
void ili_mspi_set_slave_to_qual_mode(void);
int ili_mspi_write_slave_register(u32 addr, u32 data, int len);
void ili_mspi_read_slave_register(u32 addr, u32 *data, int len);
int ili_spi_ms_register_read(u32 addr, u32 *data, int len);
int ili_spi_ice_mode_read(u32 addr, u32 *data, int len);
int ili_wdt_reset_status(bool enter_ice, bool exit_ice);
void ili_ic_edge_palm_para_init(void);
void ili_ic_send_edge_palm_para(void);
int ili_ic_func_ctrl(const char *name, int ctrl);
void ili_ic_func_ctrl_reset(void);
int ili_ic_code_reset(bool mcu);
int ili_get_dma1_config(u32 *reg_data);
int ili_set_dma1_config(u32 reg_data);
int ili_ic_whole_reset(bool mcu, bool withflash);
int ili_ic_set_cascade_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu);
int ili_ic_get_cascade_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu);
int ili_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu);
int ili_ic_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu);
u32 ili_ic_get_pc_counter(int stat);
int ili_ic_int_trigger_ctrl(bool pulse);
int ili_ic_check_int_level(bool level);
int ili_ic_check_int_pulse(bool pulse);
int ili_ic_check_busy(int count, int delay, bool irq);
int ili_ic_get_project_id(u8 *pdata, int size);
int ili_ic_get_core_ver(void);
int ili_fw_uart_ctrl(u8 ctrl);
int ili_ic_get_fw_ver(void);
int ili_ic_get_panel_info(void);
int ili_ic_get_tp_info(void);
int ili_ic_get_compress_info(void);
int ili_ic_get_all_info(void);
int ili_ic_get_protocl_ver(void);
int ili_ic_get_info(void);
void ili_ic_init(void);
int ili_move_mp_code_iram(void);
int ili_proximity_near(int mode);
int ili_proximity_far(int mode);
int ili_move_gesture_code_flash(int mode);
int ili_set_gesture_symbol(void);
int ili_move_gesture_code_iram(int mode);
void ili_dump_data(void *data, int type, int len, int row_len, const char *name);
int ili_ic_dummy_check(void);

/* ilitek_v3_touch.c */
u8 ili_calc_packet_checksum(u8 *packet, int len);
int ili_touch_esd_gesture_flash(void);
int ili_touch_esd_gesture_iram(void);
void ili_demo_debug_info_id0(u8 *buf, size_t len);
void ili_demo_debug_info_mode(u8 *buf, size_t len);
void ili_report_ap_mode(u8 *buf, int len);
void ili_touch_press(u16 x, u16 y, u16 pressure, u16 id);
void ili_touch_release(u16 x, u16 y, u16 id);
void ili_touch_release_all_point(void);
void ili_pen_demo_mode_report_point(u8 *buf, int len);
void ili_report_pen_ap_mode(u8 *buf, int len);
void ili_pen_debug_mode_report_point(u8 *buf, int len);
void ili_debug_mode_report_point(u8 *buf, int len);
void ili_report_debug_mode(u8 *buf, int len);
void ili_report_pen_debug_mode(u8 *buf, int len);
void ili_report_debug_lite_mode(u8 *buf, int len);
void ili_report_proximity_mode(u8 *buf, int len);
void ili_report_gesture_mode(u8 *buf, int len);
void ili_report_i2cuart_mode(u8 *buf, int len);

/* ilitek_v3_mp.c */
int ilitek_tddi_mp_ini_parser(const char *path);
int ili_parsing_sram_param(void);
int ili_mp_test_main(char *apk, bool lcm_on);

/* ilitek_v3_hostdl.c */
int ili_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu);
int ili_fw_upgrade(int op);
void ili_fw_read_flash_info(bool mcu);
int ilitek_tddi_flash_protect(bool enable, bool mcu);
void ili_flash_clear_dma(void);
void ili_flash_dma_write(u32 start, u32 end, u32 len);
int ili_fw_dump_flash_data(u32 start, u32 end, bool user, bool mcu);

/* ilitek_v3_node.c */
void ili_str2hex(char *str);
int ili_katoi(char *str);
int file_write(struct file *file, loff_t *pos, const char *data, size_t len);
int ili_cascade_rw_tp_reg(u32 mcu, u32 type, u32 addr, u32 wdata, u32 wlen, u32 rlen);
int ili_get_tp_recore_ctrl(int data);
int ili_get_tp_recore_data(bool mcu);
void ili_gesture_fail_reason(bool enable);
int ili_tp_data_mode_ctrl(u8 *cmd);
int ili_mp_lcm_off_env_ctrl(int mode);
int tran_get_nvt_selftest_result(void);
void ili_node_init(void);

#endif /* __ILITEK_V3_H */
