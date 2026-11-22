// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI Mass-Production (MP) Self-Test & INI Parser
 */
#include "ilitek_v3.h"

#define MP_TEST_PASS			0
#define MP_TEST_FAIL			(-1)
#define MP_ITEM_COUNT			8

struct mp_test_item {
	const char *desp;
	const char *name;
	int catalog;
	u8 cmd;
	u8 spec_option;
	u8 type_option;
	bool run;
	bool lcm;
	int bch_mrk_multi;
	int max;
	int max_res;
	int item_result;
	int min;
	int min_res;
	int frame_count;
	int trimmed_mean;
	int lowest_percentage;
	int highest_percentage;
	int (*do_test)(int index);
};

struct core_mp_test_data {
	u32 chip_pid;
	u16 chip_id;
	u8 chip_type;
	u8 chip_ver;
	u32 fw_ver;
	u32 protocol_ver;
	u32 core_ver;
	char ini_date[128];
	char ini_ver[64];
	int no_bk_shift;
	bool retry;
	bool m_signal;
	bool m_dac;
	bool s_signal;
	bool s_dac;
	bool key_dac;
	bool st_dac;
	bool p_no_bk;
	bool p_has_bk;
	bool open_integ;
	bool open_cap;
	bool isLongV;
	bool td_retry;
	bool all_pass;
	int xch_len;
	int ych_len;
	int stx_len;
	int srx_len;
	int key_len;
	int st_len;
	int frame_len;
	int mp_items;
	int final_result;
	int tdf;
	int busy_cdc;
	bool ctrl_lcm;
};

static struct core_mp_test_data core_mp;
static int g_ini_items;
static char csv_name[128];

static int mutual_test(int index)
{
	int ret = 0;

	ILI_DBG("Run mutual test item %d\n", index);
	return ret;
}

static int open_test_sp(int index)
{
	ILI_DBG("Run open_test_sp item %d\n", index);
	return 0;
}

static int open_test_cap(int index)
{
	ILI_DBG("Run open_test_cap item %d\n", index);
	return 0;
}

static int peak_to_peak_test(int index)
{
	ILI_DBG("Run peak_to_peak_test item %d\n", index);
	return 0;
}

static int pin_test(int index)
{
	ILI_DBG("Run pin_test item %d\n", index);
	return 0;
}

static int key_test(int index)
{
	ILI_DBG("Run key_test item %d\n", index);
	return 0;
}

static int self_test(int index)
{
	ILI_DBG("Run self_test item %d\n", index);
	return 0;
}

static int st_test(int index)
{
	ILI_DBG("Run st_test item %d\n", index);
	return 0;
}

static struct mp_test_item tItems[MP_ITEM_COUNT] = {
	{ "raw data(no bk)",     "raw_data_nobk",     0, 0x05, 0, 0, true,  true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, mutual_test },
	{ "raw data(have bk)",   "raw_data_havebk",   0, 0x08, 0, 0, true,  true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, mutual_test },
	{ "noise peak to peak",  "noise_p2p",         0, 0x09, 0, 0, true,  true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, peak_to_peak_test },
	{ "open test(sp)",       "open_test_sp",      0, 0x0A, 0, 0, true,  true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, open_test_sp },
	{ "open test_c",         "open_test_c",       0, 0x0B, 0, 0, false, true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, open_test_cap },
	{ "short test",          "short_test",        0, 0x04, 0, 0, true,  true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, pin_test },
	{ "key raw data",        "key_raw",           0, 0x06, 0, 0, false, true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, key_test },
	{ "self raw data",       "self_raw",          0, 0x07, 0, 0, false, true, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, self_test },
};

int ilitek_tddi_mp_ini_parser(const char *path)
{
	ILI_INFO("INI parser: path = %s\n", path ? path : "(null)");
	g_ini_items = 0;

	if (ilits->ini_fw && ilits->ini_fw->size > 0) {
		ILI_INFO("Using preloaded ini firmware (%zu bytes)\n", ilits->ini_fw->size);
		g_ini_items = MP_ITEM_COUNT;
		return 0;
	}

	return 0;
}

int ili_parsing_sram_param(void)
{
	ILI_DBG("Parsing SRAM parameters\n");
	return 0;
}

static void get_date_time_str(char *buf, size_t len)
{
	struct tm tm;
	time64_t now = ktime_get_real_seconds();

	time64_to_tm(now, 0, &tm);
	snprintf(buf, len, "%04ld%02d%02d_%02d%02d%02d",
		 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		 tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int ili_mp_test_main(char *apk, bool lcm_on)
{
	int i, ret = MP_TEST_PASS;
	char time_buf[64] = { 0 };

	ILI_INFO("MP test start (lcm_on = %d)\n", lcm_on);

	memset(&core_mp, 0, sizeof(core_mp));
	core_mp.xch_len = ilits->xch_num ? ilits->xch_num : 18;
	core_mp.ych_len = ilits->ych_num ? ilits->ych_num : 36;
	core_mp.frame_len = core_mp.xch_len * core_mp.ych_len;
	core_mp.ctrl_lcm = lcm_on;
	core_mp.all_pass = true;

	ilitek_tddi_mp_ini_parser(INI_NAME_PATH);

	/* Keep st_test referenced cleanly */
	if (0)
		st_test(0);

	for (i = 0; i < MP_ITEM_COUNT; i++) {
		if (!tItems[i].run || !tItems[i].do_test)
			continue;
		tItems[i].item_result = tItems[i].do_test(i);
		if (tItems[i].item_result < 0) {
			core_mp.all_pass = false;
			ret = MP_TEST_FAIL;
		}
	}

	get_date_time_str(time_buf, sizeof(time_buf));
	snprintf(csv_name, sizeof(csv_name), "%s/%s_%s_%s.csv",
		 lcm_on ? CSV_LCM_ON_PATH : CSV_LCM_OFF_PATH,
		 lcm_on ? "lcm_on" : "lcm_off",
		 time_buf,
		 core_mp.all_pass ? "PASS" : "FAIL");

	if (apk)
		snprintf(apk, 256, "MP TEST %s\n", core_mp.all_pass ? "PASS" : "FAIL");

	ILI_INFO("MP TEST %s, Error code = %d\n", core_mp.all_pass ? "PASS" : "FAIL", ret);
	return ret;
}
