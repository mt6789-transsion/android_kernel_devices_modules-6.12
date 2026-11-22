// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech FT8725 Factory Self-Test Implementation
 */
#include "focaltech_test.h"

static int ft8725_short_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int i = 0;
	int *adcdata = NULL;
	int ch_num = tdata->node.channel_num;
	int byte_num = ch_num * 2;
	bool result = true;

	FTS_TEST_INFO("======= Test Item: Short Circuit Test");
	adcdata = fts_malloc(ch_num * sizeof(int));
	if (!adcdata)
		return -ENOMEM;

	ret = short_get_adcdata_incell(0x04, ch_num, byte_num, adcdata);
	if (ret < 0) {
		result = false;
		goto out;
	}

	for (i = 0; i < ch_num; i++) {
		if (adcdata[i] < tdata->thr.short_res_min) {
			result = false;
			break;
		}
	}

	fts_test_save_data("Short Test", 0, adcdata, ch_num, false, false, result);
out:
	fts_free_proc(adcdata);
	*test_result = result;
	FTS_TEST_INFO("------ Short Test %s", result ? "PASS" : "NG");
	return ret;
}

static int ft8725_cb_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int *cbdata = NULL;
	int byte_num = tdata->node.node_num;
	bool result = false;

	FTS_TEST_INFO("======= Test Item: CB Test");
	cbdata = fts_malloc(byte_num * sizeof(int));
	if (!cbdata)
		return -ENOMEM;

	ret = chip_clb();
	if (ret < 0)
		goto out;

	ret = get_cb_incell(0, byte_num, cbdata);
	if (ret < 0)
		goto out;

	result = compare_data(cbdata, tdata->thr.cb_min, tdata->thr.cb_max, 0, 0, false);
	fts_test_save_data("CB Test", 1, cbdata, byte_num, false, false, result);

out:
	fts_free_proc(cbdata);
	*test_result = result;
	FTS_TEST_INFO("------ CB Test %s", result ? "PASS" : "NG");
	return ret;
}

static int ft8725_rawdata_test(struct fts_test *tdata, bool *test_result)
{
	int ret = 0;
	int *rawdata = NULL;
	int node_num = tdata->node.node_num;
	bool result = false;

	FTS_TEST_INFO("======= Test Item: RawData Test");
	rawdata = fts_malloc(node_num * sizeof(int));
	if (!rawdata)
		return -ENOMEM;

	ret = start_scan();
	if (ret < 0)
		goto out;

	ret = get_rawdata(rawdata);
	if (ret < 0)
		goto out;

	result = compare_data(rawdata, tdata->thr.rawdata_min, tdata->thr.rawdata_max,
			      0, 0, false);
	fts_test_save_data("RawData Test", 2, rawdata, node_num, false, false, result);

out:
	fts_free_proc(rawdata);
	*test_result = result;
	FTS_TEST_INFO("------ RawData Test %s", result ? "PASS" : "NG");
	return ret;
}

static int start_test_ft8725(void)
{
	int ret = 0;
	struct fts_test *tdata = fts_ftest;
	bool temp_result = false;
	bool test_result = true;

	if (!tdata)
		return -EINVAL;

	if (tdata->item.short_test) {
		ret = ft8725_short_test(tdata, &temp_result);
		if (ret < 0 || !temp_result)
			test_result = false;
	}

	if (tdata->item.cb_test) {
		ret = ft8725_cb_test(tdata, &temp_result);
		if (ret < 0 || !temp_result)
			test_result = false;
	}

	if (tdata->item.rawdata_test) {
		ret = ft8725_rawdata_test(tdata, &temp_result);
		if (ret < 0 || !temp_result)
			test_result = false;
	}

	return test_result ? 0 : -EIO;
}

struct test_funcs test_func_ft8725 = {
	.ctype = { 0x29 },
	.hwtype = 1,
	.start_test = start_test_ft8725,
};
