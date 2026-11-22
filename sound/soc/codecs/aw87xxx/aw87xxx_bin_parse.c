/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "aw87xxx_bin_parse.h"

#define AWINIC_CODE_VERSION		"V0.0.7-V1.0.4"

#define BIN_LOGI(fmt, ...) \
	pr_info("AWINIC_BIN %s,line= %d," fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#define BIN_LOGE(fmt, ...) \
	pr_err("AWINIC_BIN_ERR %s,line= %d," fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#define GET_32_DATA(w, x, y, z) \
	((unsigned int)((((unsigned int)(w)) << 24) | \
			(((unsigned int)(x)) << 16) | \
			(((unsigned int)(y)) << 8)  | \
			((unsigned int)(z))))

int aw_check_sum(struct aw_bin *bin, int bin_num)
{
	unsigned int i = 0;
	unsigned int sum_data = 0;
	unsigned int check_sum = 0;
	unsigned char *p_check_sum = NULL;

	BIN_LOGI("enter");

	if (bin_num < 0 || bin_num >= BIN_NUM_MAX)
		return -EINVAL;

	p_check_sum = &(bin->info.data[(bin->header_info[bin_num].valid_data_addr -
					bin->header_info[bin_num].header_len)]);
	BIN_LOGI("aw_bin_parse p_check_sum = %p", p_check_sum);

	for (i = 4; i < bin->header_info[bin_num].bin_data_len +
		      bin->header_info[bin_num].header_len; i++) {
		sum_data += p_check_sum[i];
	}

	BIN_LOGI("aw_bin_parse bin_num=%d, check_sum = 0x%x, sum_data = 0x%x",
		 bin_num, bin->header_info[bin_num].check_sum, sum_data);

	check_sum = GET_32_DATA(p_check_sum[3], p_check_sum[2],
				p_check_sum[1], p_check_sum[0]);
	if (sum_data != check_sum) {
		BIN_LOGE("aw_bin_parse check sum or check bin data len error");
		BIN_LOGE("aw_bin_parse bin_num=%d, check_sum = 0x%x, sum_data = 0x%x",
			 bin_num, check_sum, sum_data);
		return -1;
	}

	return 0;
}

int aw_check_data_version(struct aw_bin *bin, int bin_num)
{
	int i = 0;

	BIN_LOGI("enter");

	if (bin_num < 0 || bin_num >= BIN_NUM_MAX)
		return -EINVAL;

	for (i = DATA_VERSION_V1; i < DATA_VERSION_MAX; i++) {
		if (bin->header_info[bin_num].bin_data_ver == i)
			return 0;
	}

	BIN_LOGE("aw_bin_parse Unrecognized this bin data version");
	return -3;
}

int aw_check_register_num_v1(struct aw_bin *bin, int bin_num)
{
	unsigned int check_register_num = 0;
	unsigned int parse_register_num = 0;
	char *p_check_sum = NULL;

	BIN_LOGI("enter");

	if (bin_num < 0 || bin_num >= BIN_NUM_MAX)
		return -EINVAL;

	p_check_sum = (char *)&(bin->info.data[bin->header_info[bin_num].valid_data_addr]);
	BIN_LOGI("aw_bin_parse p_check_sum = %p", p_check_sum);

	parse_register_num = GET_32_DATA((unsigned char)p_check_sum[3],
					 (unsigned char)p_check_sum[2],
					 (unsigned char)p_check_sum[1],
					 (unsigned char)p_check_sum[0]);

	bin->header_info[bin_num].reg_data_byte_len =
		bin->header_info[bin_num].reg_byte_len +
		bin->header_info[bin_num].data_byte_len;

	if (bin->header_info[bin_num].reg_data_byte_len == 0)
		return -4;

	check_register_num = (bin->header_info[bin_num].bin_data_len - 4) /
			     bin->header_info[bin_num].reg_data_byte_len;

	BIN_LOGI("aw_bin_parse bin_num=%d, parse_register_num = 0x%x, check_register_num = 0x%x",
		 bin_num, parse_register_num, check_register_num);

	if (parse_register_num != check_register_num) {
		BIN_LOGE("aw_bin_parse register num is error");
		BIN_LOGE("aw_bin_parse bin_num=%d, parse_register_num = 0x%x, check_register_num = 0x%x",
			 bin_num, parse_register_num, check_register_num);
		return -4;
	}

	bin->header_info[bin_num].reg_num = parse_register_num;
	bin->header_info[bin_num].valid_data_len =
		bin->header_info[bin_num].bin_data_len - 4;
	bin->header_info[bin_num].valid_data_addr += 4;

	return 0;
}

int aw_check_dsp_reg_num_v1(struct aw_bin *bin, int bin_num)
{
	unsigned int check_dsp_reg_num = 0;
	unsigned int parse_dsp_reg_num = 0;
	char *p_check_sum = NULL;

	BIN_LOGI("enter");

	if (bin_num < 0 || bin_num >= BIN_NUM_MAX)
		return -EINVAL;

	p_check_sum = (char *)&(bin->info.data[bin->header_info[bin_num].valid_data_addr]);
	BIN_LOGI("aw_bin_parse p_check_sum = %p", p_check_sum);

	parse_dsp_reg_num = GET_32_DATA((unsigned char)p_check_sum[7],
					(unsigned char)p_check_sum[6],
					(unsigned char)p_check_sum[5],
					(unsigned char)p_check_sum[4]);

	bin->header_info[bin_num].reg_data_byte_len =
		bin->header_info[bin_num].data_byte_len;

	if (bin->header_info[bin_num].reg_data_byte_len == 0)
		return -5;

	check_dsp_reg_num = (bin->header_info[bin_num].bin_data_len - 12) /
			    bin->header_info[bin_num].reg_data_byte_len;

	BIN_LOGI("aw_bin_parse bin_num=%d, parse_dsp_reg_num = 0x%x, check_dsp_reg_num = 0x%x",
		 bin_num, parse_dsp_reg_num, check_dsp_reg_num);

	if (parse_dsp_reg_num != check_dsp_reg_num) {
		BIN_LOGE("aw_bin_parse dsp reg num is error");
		BIN_LOGE("aw_bin_parse bin_num=%d, parse_dsp_reg_num = 0x%x, check_dsp_reg_num = 0x%x",
			 bin_num, parse_dsp_reg_num, check_dsp_reg_num);
		return -5;
	}

	bin->header_info[bin_num].download_addr =
		GET_32_DATA((unsigned char)p_check_sum[3],
			    (unsigned char)p_check_sum[2],
			    (unsigned char)p_check_sum[1],
			    (unsigned char)p_check_sum[0]);
	bin->header_info[bin_num].reg_num = parse_dsp_reg_num;
	bin->header_info[bin_num].valid_data_len =
		bin->header_info[bin_num].bin_data_len - 12;
	bin->header_info[bin_num].valid_data_addr += 12;

	return 0;
}

int aw_check_soc_app_num_v1(struct aw_bin *bin, int bin_num)
{
	unsigned int check_soc_app_num = 0;
	unsigned int parse_soc_app_num = 0;
	char *p_check_sum = NULL;

	BIN_LOGI("enter");

	if (bin_num < 0 || bin_num >= BIN_NUM_MAX)
		return -EINVAL;

	p_check_sum = (char *)&(bin->info.data[bin->header_info[bin_num].valid_data_addr]);
	BIN_LOGI("aw_bin_parse p_check_sum = %p", p_check_sum);

	bin->header_info[bin_num].app_version =
		GET_32_DATA((unsigned char)p_check_sum[3],
			    (unsigned char)p_check_sum[2],
			    (unsigned char)p_check_sum[1],
			    (unsigned char)p_check_sum[0]);

	parse_soc_app_num = GET_32_DATA((unsigned char)p_check_sum[11],
					(unsigned char)p_check_sum[10],
					(unsigned char)p_check_sum[9],
					(unsigned char)p_check_sum[8]);
	check_soc_app_num = bin->header_info[bin_num].bin_data_len - 12;

	BIN_LOGI("aw_bin_parse bin_num=%d, parse_soc_app_num = 0x%x, check_soc_app_num = 0x%x",
		 bin_num, parse_soc_app_num, check_soc_app_num);

	if (parse_soc_app_num != check_soc_app_num) {
		BIN_LOGE("aw_bin_parse soc app num is error");
		BIN_LOGE("aw_bin_parse bin_num=%d, parse_soc_app_num = 0x%x, check_soc_app_num = 0x%x",
			 bin_num, parse_soc_app_num, check_soc_app_num);
		return -7;
	}

	bin->header_info[bin_num].reg_num = parse_soc_app_num;
	bin->header_info[bin_num].download_addr =
		GET_32_DATA((unsigned char)p_check_sum[7],
			    (unsigned char)p_check_sum[6],
			    (unsigned char)p_check_sum[5],
			    (unsigned char)p_check_sum[4]);
	bin->header_info[bin_num].valid_data_len =
		bin->header_info[bin_num].bin_data_len - 12;
	bin->header_info[bin_num].valid_data_addr += 12;

	return 0;
}

void aw_get_single_bin_header_1_0_0(struct aw_bin *bin)
{
	int i = 0;
	unsigned int idx = bin->all_bin_parse_num;

	BIN_LOGI("enter %s", __func__);

	if (idx >= BIN_NUM_MAX)
		return;

	bin->header_info[idx].header_len = HEADER_LEN;
	bin->header_info[idx].check_sum =
		GET_32_DATA((unsigned char)bin->p_addr[3],
			    (unsigned char)bin->p_addr[2],
			    (unsigned char)bin->p_addr[1],
			    (unsigned char)bin->p_addr[0]);
	bin->header_info[idx].header_ver =
		GET_32_DATA((unsigned char)bin->p_addr[7],
			    (unsigned char)bin->p_addr[6],
			    (unsigned char)bin->p_addr[5],
			    (unsigned char)bin->p_addr[4]);
	bin->header_info[idx].bin_data_type =
		GET_32_DATA((unsigned char)bin->p_addr[11],
			    (unsigned char)bin->p_addr[10],
			    (unsigned char)bin->p_addr[9],
			    (unsigned char)bin->p_addr[8]);
	bin->header_info[idx].bin_data_ver =
		GET_32_DATA((unsigned char)bin->p_addr[15],
			    (unsigned char)bin->p_addr[14],
			    (unsigned char)bin->p_addr[13],
			    (unsigned char)bin->p_addr[12]);
	bin->header_info[idx].bin_data_len =
		GET_32_DATA((unsigned char)bin->p_addr[19],
			    (unsigned char)bin->p_addr[18],
			    (unsigned char)bin->p_addr[17],
			    (unsigned char)bin->p_addr[16]);
	bin->header_info[idx].ui_ver =
		GET_32_DATA((unsigned char)bin->p_addr[23],
			    (unsigned char)bin->p_addr[22],
			    (unsigned char)bin->p_addr[21],
			    (unsigned char)bin->p_addr[20]);
	for (i = 0; i < 8; i++)
		bin->header_info[idx].chip_type[i] = bin->p_addr[24 + i];
	bin->header_info[idx].reg_byte_len =
		GET_32_DATA((unsigned char)bin->p_addr[35],
			    (unsigned char)bin->p_addr[34],
			    (unsigned char)bin->p_addr[33],
			    (unsigned char)bin->p_addr[32]);
	bin->header_info[idx].data_byte_len =
		GET_32_DATA((unsigned char)bin->p_addr[39],
			    (unsigned char)bin->p_addr[38],
			    (unsigned char)bin->p_addr[37],
			    (unsigned char)bin->p_addr[36]);
	bin->header_info[idx].device_addr =
		GET_32_DATA((unsigned char)bin->p_addr[43],
			    (unsigned char)bin->p_addr[42],
			    (unsigned char)bin->p_addr[41],
			    (unsigned char)bin->p_addr[40]);

	bin->header_info[idx].valid_data_len = 0;
	bin->header_info[idx].reg_num = 0;
	bin->header_info[idx].reg_data_byte_len = 0;
	bin->header_info[idx].download_addr = 0;
	bin->header_info[idx].app_version = 0;

	bin->all_bin_parse_num += 1;
}

int aw_parse_each_of_multi_bins_1_0_0(unsigned int bin_num, int bin_serial_num,
				      struct aw_bin *bin)
{
	int ret = 0;
	unsigned int bin_start_addr = 0;
	unsigned int valid_data_len = 0;

	BIN_LOGI("aw_bin_parse enter multi bin branch -- %s", __func__);

	if (!bin_serial_num) {
		bin_start_addr =
			GET_32_DATA((unsigned char)bin->p_addr[67],
				    (unsigned char)bin->p_addr[66],
				    (unsigned char)bin->p_addr[65],
				    (unsigned char)bin->p_addr[64]);
		bin->p_addr += (60 + bin_start_addr);
		bin->header_info[bin->all_bin_parse_num].valid_data_addr =
			bin->header_info[bin->all_bin_parse_num - 1].valid_data_addr +
			4 + 8 * bin_num + 60;
	} else {
		valid_data_len =
			bin->header_info[bin->all_bin_parse_num - 1].bin_data_len;
		bin->p_addr += (60 + valid_data_len);
		bin->header_info[bin->all_bin_parse_num].valid_data_addr =
			bin->header_info[bin->all_bin_parse_num - 1].valid_data_addr +
			bin->header_info[bin->all_bin_parse_num - 1].bin_data_len + 60;
	}

	ret = aw_parse_bin_header_1_0_0(bin);
	return ret;
}

int aw_get_multi_bin_header_1_0_0(struct aw_bin *bin)
{
	int i = 0;
	int ret = 0;
	unsigned int bin_num = 0;

	BIN_LOGI("enter %s", __func__);

	bin_num = GET_32_DATA((unsigned char)bin->p_addr[63],
			      (unsigned char)bin->p_addr[62],
			      (unsigned char)bin->p_addr[61],
			      (unsigned char)bin->p_addr[60]);

	if (bin->multi_bin_parse_num == 1)
		bin->header_info[bin->all_bin_parse_num].valid_data_addr = 60;

	aw_get_single_bin_header_1_0_0(bin);

	for (i = 0; i < (int)bin_num; i++) {
		BIN_LOGI("aw_bin_parse enter multi bin for is %d", i);
		ret = aw_parse_each_of_multi_bins_1_0_0(bin_num, i, bin);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int aw_parse_bin_header_1_0_0(struct aw_bin *bin)
{
	int ret = 0;
	unsigned int bin_data_type = 0;

	BIN_LOGI("enter %s", __func__);

	bin_data_type = GET_32_DATA((unsigned char)bin->p_addr[11],
				    (unsigned char)bin->p_addr[10],
				    (unsigned char)bin->p_addr[9],
				    (unsigned char)bin->p_addr[8]);
	BIN_LOGI("aw_bin_parse bin_data_type 0x%x", bin_data_type);

	switch (bin_data_type) {
	case DATA_TYPE_REGISTER:
	case DATA_TYPE_DSP_REG:
	case DATA_TYPE_SOC_APP:
		bin->single_bin_parse_num += 1;
		BIN_LOGI("%s bin->single_bin_parse_num is %d",
			 __func__, bin->single_bin_parse_num);
		if (!bin->multi_bin_parse_num)
			bin->header_info[bin->all_bin_parse_num].valid_data_addr = 60;
		aw_get_single_bin_header_1_0_0(bin);
		break;
	case DATA_TYPE_MULTI_BINS:
		bin->multi_bin_parse_num += 1;
		BIN_LOGI("%s bin->multi_bin_parse_num is %d",
			 __func__, bin->multi_bin_parse_num);
		ret = aw_get_multi_bin_header_1_0_0(bin);
		if (ret < 0)
			return ret;
		break;
	default:
		BIN_LOGI("%s, data type is %d", __func__, bin_data_type);
		break;
	}

	return 0;
}

static int aw_parse_bin_file_v_1_0_0(struct aw_bin *bin)
{
	int i = 0;
	int ret = 0;

	ret = aw_parse_bin_header_1_0_0(bin);
	if (ret < 0)
		return ret;

	for (i = 0; i < (int)bin->all_bin_parse_num; i++) {
		if (bin->header_info[i].bin_data_type == DATA_TYPE_MULTI_BINS)
			continue;

		ret = aw_check_sum(bin, i);
		if (ret < 0)
			return ret;

		ret = aw_check_data_version(bin, i);
		if (ret < 0)
			return ret;

		if (bin->header_info[i].bin_data_ver == DATA_VERSION_V1) {
			switch (bin->header_info[i].bin_data_type) {
			case DATA_TYPE_REGISTER:
				ret = aw_check_register_num_v1(bin, i);
				if (ret < 0)
					return ret;
				break;
			case DATA_TYPE_DSP_REG:
				ret = aw_check_dsp_reg_num_v1(bin, i);
				if (ret < 0)
					return ret;
				break;
			case DATA_TYPE_SOC_APP:
				ret = aw_check_soc_app_num_v1(bin, i);
				if (ret < 0)
					return ret;
				break;
			default:
				bin->header_info[i].valid_data_len =
					bin->header_info[i].bin_data_len;
				break;
			}
		}
	}

	return 0;
}

int aw_parsing_bin_file(struct aw_bin *bin)
{
	int ret = 0;
	unsigned int header_ver = 0;

	BIN_LOGI("aw_bin_parse code version:%s", AWINIC_CODE_VERSION);

	if (!bin) {
		BIN_LOGE("aw_bin_parse bin is NULL");
		return -EINVAL;
	}

	bin->p_addr = (char *)bin->info.data;
	bin->all_bin_parse_num = 0;
	bin->multi_bin_parse_num = 0;
	bin->single_bin_parse_num = 0;

	header_ver = GET_32_DATA((unsigned char)bin->p_addr[7],
				 (unsigned char)bin->p_addr[6],
				 (unsigned char)bin->p_addr[5],
				 (unsigned char)bin->p_addr[4]);

	BIN_LOGI("aw_bin_parse header version 0x%x", header_ver);

	switch (header_ver) {
	case Header_Version_1_0_0:
		ret = aw_parse_bin_file_v_1_0_0(bin);
		if (ret < 0) {
			BIN_LOGE("aw_bin_parse bin file error, ret=%d", ret);
			return ret;
		}
		break;
	default:
		BIN_LOGE("aw_bin_parse Unrecognized this bin header version");
		return -2;
	}

	return 0;
}
