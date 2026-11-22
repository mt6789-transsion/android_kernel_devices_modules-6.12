/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_BIN_PARSE_H__
#define __AW87XXX_BIN_PARSE_H__

#include <linux/types.h>

#define BIN_NUM_MAX			100
#define HEADER_LEN			60
#define BIN_HEADER_VER_1_0_0		0x01000000

#define HEADER_VERSION_1_0_0		0x01000000

enum bin_header_version_enum {
	Header_Version_1_0_0 = 0x01000000,
};

enum data_type_enum {
	DATA_TYPE_REGISTER = 0x00000000,
	DATA_TYPE_DSP_REG = 0x00000010,
	DATA_TYPE_DSP_CFG = 0x00000011,
	DATA_TYPE_SOC_REG = 0x00000020,
	DATA_TYPE_SOC_APP = 0x00000021,
	DATA_TYPE_MULTI_BINS = 0x00002000,
};

enum data_version_enum {
	DATA_VERSION_V1 = 0x00000001,
	DATA_VERSION_MAX,
};

struct bin_header_info {
	unsigned int header_len;
	unsigned int check_sum;
	unsigned int header_ver;
	unsigned int bin_data_type;
	unsigned int bin_data_ver;
	unsigned int bin_data_len;
	unsigned int ui_ver;
	unsigned char chip_type[8];
	unsigned int reg_byte_len;
	unsigned int data_byte_len;
	unsigned int device_addr;
	unsigned int valid_data_len;
	unsigned int valid_data_addr;
	unsigned int reg_num;
	unsigned int reg_data_byte_len;
	unsigned int download_addr;
	unsigned int app_version;
};

struct bin_container {
	unsigned int len;
	unsigned char data[];
};

struct aw_bin {
	char *p_addr;
	unsigned int all_bin_parse_num;
	unsigned int multi_bin_parse_num;
	unsigned int single_bin_parse_num;
	struct bin_header_info header_info[BIN_NUM_MAX];
	struct bin_container info;
};

int aw_check_sum(struct aw_bin *bin, int bin_num);
int aw_check_data_version(struct aw_bin *bin, int bin_num);
int aw_check_register_num_v1(struct aw_bin *bin, int bin_num);
int aw_check_dsp_reg_num_v1(struct aw_bin *bin, int bin_num);
int aw_check_soc_app_num_v1(struct aw_bin *bin, int bin_num);
void aw_get_single_bin_header_1_0_0(struct aw_bin *bin);
int aw_parse_each_of_multi_bins_1_0_0(unsigned int bin_num, int bin_serial_num,
				      struct aw_bin *bin);
int aw_parse_bin_header_1_0_0(struct aw_bin *bin);
int aw_get_multi_bin_header_1_0_0(struct aw_bin *bin);
int aw_parsing_bin_file(struct aw_bin *bin);

#endif /* __AW87XXX_BIN_PARSE_H__ */
