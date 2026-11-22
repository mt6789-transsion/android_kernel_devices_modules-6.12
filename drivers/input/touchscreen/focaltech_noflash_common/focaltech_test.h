/* SPDX-License-Identifier: GPL-2.0 */
/*
 * FocalTech Factory Self-Test Framework Header
 */
#ifndef _FOCALTECH_TEST_H
#define _FOCALTECH_TEST_H

#include "focaltech_core.h"

#define FTS_TEST_INFO(fmt, args...) \
	printk(KERN_INFO "[FTS_TS/I][TEST]%s:" fmt "\n", __func__, ##args)

#define FTS_TEST_ERROR(fmt, args...) \
	printk(KERN_ERR "[FTS_TS/E][TEST]%s:" fmt "\n", __func__, ##args)

#define FTS_TEST_DBG(fmt, args...) \
	printk(KERN_DEBUG "[FTS_TS][TEST]%s:" fmt "\n", __func__, ##args)

#define FTS_TEST_FUNC_ENTER() \
	printk(KERN_DEBUG "[FTS_TS][TEST]%s: Enter\n", __func__)

#define FTS_TEST_FUNC_EXIT() \
	printk(KERN_DEBUG "[FTS_TS][TEST]%s: Exit(%d)\n", __func__, __LINE__)

#define DEVIDE_MODE_ADDR		0x00
#define REG_LINE_NUM			0x01
#define REG_TX_NUM			0x02
#define REG_RX_NUM			0x03
#define REG_RAWBUF0			0x36
#define REG_CB_ADDR_H			0x18
#define REG_CB_ADDR_L			0x19
#define REG_CB_BUF0			0x6E
#define REG_CLB				0x04

#define FACTORY_REG_OPEN_START		0x15
#define FACTORY_REG_OPEN_STATE		0x16
#define FACTORY_REG_OPEN_ADDR		0xCF
#define FACTORY_REG_OPEN_IDLE		0x03

#define FACTORY_REG_SHORT_TEST_EN	0x0F
#define FACTORY_REG_SHORT_TEST_STATE	0x10
#define FACTORY_REG_SHORT_ADDR		0x89

#define FACTORY_REG_LCD_NOISE_START	0x11
#define FACTORY_REG_LCD_NOISE_FRAME	0x12
#define FACTORY_REG_LCD_NOISE_TEST_STATE 0x13
#define FACTORY_REG_LCD_NOISE_TTHR	0x14

#define CSV_BUFFER_LEN			(1024 * 80)
#define TEST_RESULT_LENGTH		(1024 * 8)

enum {
	RESULT_NULL = 0,
	RESULT_PASS = 1,
	RESULT_NG = 2,
};

struct incell_threshold_b {
	int short_res_min;
	int short_va_res_min;
	int short_a_res_min;
	int open_cb_min;
	int open_k1_check;
	int open_k1_value;
	int open_k2_check;
	int open_k2_value;
	int cb_min;
	int cb_max;
	int rawdata_min;
	int rawdata_max;
	int lcdnoise_frame;
	int lcdnoise_coefficient;
};

struct incell_testitem {
	u32 short_test:1;
	u32 open_test:1;
	u32 cb_test:1;
	u32 rawdata_test:1;
	u32 lcdnoise_test:1;
};

struct fts_test_node {
	int channel_num;
	int tx_num;
	int rx_num;
	int node_num;
	int key_num;
};

struct test_funcs {
	u64 ctype[8];
	int hwtype;
	int (*param_init)(void);
	int (*start_test)(void);
	void (*save_data_private)(char *buf, int *len);
};

struct fts_test {
	struct fts_ts_data *ts_data;
	struct fts_test_node node;
	struct test_funcs *func;
	struct incell_testitem item;
	struct incell_threshold_b thr;
	struct proc_dir_entry *proc_csv;
	int *buffer;
	int buffer_length;
	char *csv_buffer;
	int csv_length;
	int test_item_code;
	int result;
};

extern struct fts_test *fts_ftest;
extern struct test_funcs test_func_ft8725;

void sys_delay(int ms);
int fts_abs(int value);
void *fts_malloc(size_t size);
void fts_free_proc(void *p);
int fts_test_read_reg(u8 addr, u8 *val);
int fts_test_write_reg(u8 addr, u8 val);
int fts_test_read(u8 addr, u8 *readbuf, int readlen);
int fts_test_write(u8 addr, u8 *writebuf, int writelen);
int enter_work_mode(void);
int enter_factory_mode(void);
int read_mass_data(u8 addr, int byte_num, int *buf);
int read_mass_data_u16(u8 addr, int byte_num, int *buf);
int wait_state_update(u8 retval);
int start_scan(void);
int get_rawdata(int *data);
int chip_clb(void);
int get_cb_incell(u16 saddr, int byte_num, int *cb_buf);
int short_get_adcdata_incell(u8 retval, u8 ch_num, int byte_num, int *adc_buf);
bool compare_data(int *data, int min, int max, int min_vk, int max_vk, bool key);
void fts_test_save_data(char *name, int code, int *data, int datacnt,
			bool mc_sc, bool key, bool result);

#endif /* _FOCALTECH_TEST_H */
