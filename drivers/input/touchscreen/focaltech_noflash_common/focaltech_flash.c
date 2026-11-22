// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech NoFlash SPI Firmware Download & Recovery
 */
#include "focaltech_core.h"

#define FTS_CMD_START1			0x55
#define FTS_CMD_START2			0xAA
#define FTS_CMD_START_DELAY		12
#define FTS_CMD_READ_ID			0x90
#define FTS_CMD_WRITE_LEN		0x7FF0
#define FTS_CMD_SET_WFLASH_ADDR		0xAD
#define FTS_CMD_WRITE_PRAM		0xAE
#define FTS_CMD_ECC_CAL			0xCC
#define FTS_CMD_ECC_READ		0xCD
#define FTS_CMD_ECC_FINISH		0xCE
#define FTS_CMD_ECC_FINISH_OK_A5	0xA5
#define FTS_CMD_START_APP		0x08
#define FTS_CMD_WRITE_PRAM_START	0x00

#define FTS_MIN_LEN			0x120
#define FTS_MAX_LEN_FILE		(256 * 1024)
#define FTS_MAX_LEN_APP			(64 * 1024)
#define FTS_MAX_LEN_SECTOR		(4 * 1024)

#define FTS_APP_INFO_OFFSET		0x100
#define FTS_FW_BIN_LEN_MIN		0x120

#define AL2_FCS_COEF			((1 << 15) + (1 << 10) + (1 << 3))

struct fts_upgrade *fwupgrade;

u8 fw_file[0];
u8 fw_file2[0];
u8 fw_file3[0];

struct upgrade_module module_list[] = {
	{ 0, "", fw_file, 0 },
	{ 0, "", fw_file2, 0 },
	{ 0, "", fw_file3, 0 },
};

struct upgrade_setting_nf upgrade_setting_list[] = {
	{ 0x87, 0x19, 0, 0x10000, 0x20000, 0x00, 0x02, 0x08, 0x01, 0x01, 0x01, 0x00, 0x00 },
	{ 0x86, 0x22, 0, 0x10000, 0x20000, 0x00, 0x02, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00 },
	{ 0x87, 0x56, 0, 0x16000, 0x07ffe, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x80, 0x09, 0, 0x16000, 0x07ffe, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x86, 0x32, 0, 0x10000, 0x20000, 0xA5, 0x01, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00 },
	{ 0x86, 0x42, 0, 0x10000, 0x20000, 0xA5, 0x01, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00 },
	{ 0x87, 0x20, 0, 0x16000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x87, 0x22, 0, 0x16000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x82, 0x01, 0, 0x18000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00 },
	{ 0xF0, 0xC6, 0, 0x15000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x56, 0x62, 0, 0x20000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x04, 0x00, 0x00, 0x05 },
	{ 0x82, 0x05, 0, 0x1E000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00 },
	{ 0x80, 0x57, 0, 0x15000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x23, 0x89, 0, 0x16000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x87, 0x25, 0, 0x16000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x01, 0x00 },
	{ 0x82, 0x06, 0, 0x1E000, 0x20000, 0xA5, 0x01, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00 },
};

static int fts_check_bootid(void)
{
	int ret = 0;
	u8 cmd = FTS_CMD_READ_ID;
	u8 id[2] = { 0 };
	struct fts_upgrade *upg = fwupgrade;

	if (!upg || !upg->ts_data || !upg->setting_nf) {
		FTS_ERROR("upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	ret = fts_read(&cmd, 1, id, 2);
	if (ret < 0) {
		FTS_ERROR("read boot id fail");
		return ret;
	}

	FTS_INFO("read boot id:0x%02x 0x%02x", id[0], id[1]);
	if ((id[0] == upg->setting_nf->rom_idh) && (id[1] == upg->setting_nf->rom_idl))
		return 0;

	return -EIO;
}

static int fts_enter_into_boot(void)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 cmd[2] = { FTS_CMD_START1, FTS_CMD_START2 };
	struct fts_upgrade *upg = fwupgrade;

	FTS_INFO("enter into boot environment");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		FTS_ERROR("upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	for (i = 0; i < 3; i++) {
		if (upg->setting_nf->delay_init) {
			fts_reset_proc(0);
			mdelay(upg->setting_nf->delay_init);
		} else {
			fts_reset_proc(0);
			mdelay(8);
		}

		for (j = 0; j < 2; j++) {
			ret = fts_write(cmd, 2);
			if (ret < 0) {
				FTS_ERROR("start cmd write fail");
				continue;
			}
			msleep(FTS_CMD_START_DELAY);
			ret = fts_check_bootid();
			if (ret == 0) {
				FTS_INFO("boot id check pass, retry=%d", i);
				return 0;
			}
		}
	}

	return -EIO;
}

static int fts_dpram_write(u32 saddr, const u8 *buf, u32 len, bool wpram)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 *cmd = NULL;
	u32 addr = 0;
	u32 offset = 0;
	u32 remainder = 0;
	u32 packet_number = 0;
	u32 packet_len = 0;
	u32 packet_size = FTS_CMD_WRITE_LEN;
	u32 baseaddr = wpram ? 0x000000 : 0x000000;

	FTS_INFO("dpram write");
	if (!buf) {
		FTS_ERROR("buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("fw length(%d) fail", len);
		return -EINVAL;
	}

	cmd = vmalloc(packet_size + 7);
	if (!cmd) {
		FTS_ERROR("malloc memory for pram write buffer fail");
		return -ENOMEM;
	}
	memset(cmd, 0, packet_size + 7);

	packet_number = len / packet_size;
	remainder = len % packet_size;
	if (remainder > 0)
		packet_number++;
	packet_len = packet_size;
	FTS_INFO("write data, num:%d remainder:%d", packet_number, remainder);

	for (i = 0; i < packet_number; i++) {
		offset = i * packet_size;
		addr = saddr + offset + baseaddr;
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		cmd[0] = FTS_CMD_SET_WFLASH_ADDR;
		cmd[1] = (addr >> 16) & 0xFF;
		cmd[2] = (addr >> 8) & 0xFF;
		cmd[3] = addr & 0xFF;
		ret = fts_write(cmd, 4);
		if (ret < 0) {
			FTS_ERROR("set write addr(%d) fail,ret=%d", i, ret);
			goto write_pram_err;
		}

		cmd[0] = FTS_CMD_WRITE_PRAM;
		for (j = 0; j < packet_len; j++)
			cmd[1 + j] = buf[offset + j];
		ret = fts_write(cmd, 1 + packet_len);
		if (ret < 0) {
			FTS_ERROR("write fw to pram(%d) fail", i);
			goto write_pram_err;
		}
	}

write_pram_err:
	vfree(cmd);
	return ret;
}

static u16 fts_ecc_calc_host(const u8 *buf, u32 len)
{
	u16 ecc = 0;
	u32 i = 0;
	u32 j = 0;
	u16 coef = 0x8408;

	for (i = 0; i < len; i += 2) {
		ecc ^= ((buf[i] << 8) | buf[i + 1]);
		for (j = 0; j < 16; j++) {
			if (ecc & 0x01)
				ecc = (ecc >> 1) ^ coef;
			else
				ecc >>= 1;
		}
	}

	return ecc;
}

static int fts_ecc_cal_tp(u32 ecc_saddr, u32 ecc_len, u16 *ecc_value)
{
	int ret = 0;
	int i = 0;
	u8 cmd[7] = { 0 };
	u8 value[2] = { 0 };
	struct fts_upgrade *upg = fwupgrade;

	if (!upg || !upg->setting_nf)
		return -EINVAL;

	cmd[0] = FTS_CMD_ECC_CAL;
	cmd[1] = (ecc_saddr >> 16) & 0xFF;
	cmd[2] = (ecc_saddr >> 8) & 0xFF;
	cmd[3] = ecc_saddr & 0xFF;
	cmd[4] = (ecc_len >> 16) & 0xFF;
	cmd[5] = (ecc_len >> 8) & 0xFF;
	cmd[6] = ecc_len & 0xFF;

	ret = fts_write(cmd, 7);
	if (ret < 0) {
		FTS_ERROR("ecc calc cmd write fail");
		return ret;
	}

	mdelay(2);
	cmd[0] = FTS_CMD_ECC_FINISH;
	for (i = 0; i < 100; i++) {
		ret = fts_read(cmd, 1, value, 1);
		if (ret < 0) {
			FTS_ERROR("ecc finish cmd read fail");
			return ret;
		}
		if (value[0] == upg->setting_nf->init_cmd)
			break;
		mdelay(1);
	}
	if (i >= 100) {
		FTS_ERROR("wait ecc finish timeout");
		return -EIO;
	}

	cmd[0] = FTS_CMD_ECC_READ;
	ret = fts_read(cmd, 1, value, 2);
	if (ret < 0) {
		FTS_ERROR("ecc read cmd fail");
		return ret;
	}

	*ecc_value = ((u16)value[0] << 8) + value[1];
	return 0;
}

static int fts_ecc_check(const u8 *buf, u32 len, u32 ecc_saddr)
{
	int ret = 0;
	int i = 0;
	u16 ecc_in_host = 0;
	u16 ecc_in_tp = 0;
	int packet_length = 0;
	int packet_number = 0;
	int packet_remainder = 0;
	int offset = 0;
	u32 packet_size = 0x20000;
	struct fts_upgrade *upg = fwupgrade;

	FTS_INFO("ecc check");
	if (!buf || !len || !upg || !upg->setting_nf) {
		FTS_ERROR("buf/len/upg is invalid");
		return -EINVAL;
	}

	if (upg->setting_nf->ecc_max_len)
		packet_size = upg->setting_nf->ecc_max_len;

	packet_number = len / packet_size;
	packet_remainder = len % packet_size;
	if (packet_remainder)
		packet_number++;
	packet_length = packet_size;

	for (i = 0; i < packet_number; i++) {
		if ((i == (packet_number - 1)) && packet_remainder)
			packet_length = packet_remainder;

		ecc_in_host = fts_ecc_calc_host(buf + offset, packet_length);
		ret = fts_ecc_cal_tp(ecc_saddr + offset, packet_length, &ecc_in_tp);
		if (ret < 0) {
			FTS_ERROR("ecc in tp calc fail");
			return ret;
		}

		FTS_DEBUG("ecc in tp:%04x,host:%04x,i:%d", ecc_in_tp, ecc_in_host, i);
		if (ecc_in_tp != ecc_in_host) {
			FTS_ERROR("ecc_in_tp(%x) != ecc_in_host(%x), ecc check fail",
				  ecc_in_tp, ecc_in_host);
			return -EIO;
		}
		offset += packet_length;
	}

	return 0;
}

static int fts_pram_write_ecc(const u8 *buf, u32 len)
{
	int ret = 0;
	u32 pram_app_size = 0;
	u16 code_len = 0;
	u16 code_len_n = 0;
	u32 pram_start_addr = 0;

	FTS_INFO("begin to write pram app(bin len:%d)", len);
	code_len = ((u16)buf[FTS_APP_INFO_OFFSET + 0] << 8) + buf[FTS_APP_INFO_OFFSET + 1];
	code_len_n = ((u16)buf[FTS_APP_INFO_OFFSET + 2] << 8) + buf[FTS_APP_INFO_OFFSET + 3];
	if ((code_len + code_len_n) != 0xFFFF) {
		FTS_ERROR("code len(%x %x) fail", code_len, code_len_n);
		return -EINVAL;
	}

	pram_app_size = ((u32)code_len) * 2;
	FTS_INFO("pram app length in fact:%d", pram_app_size);

	ret = fts_dpram_write(pram_start_addr, buf, pram_app_size, true);
	if (ret < 0) {
		FTS_ERROR("write pram fail");
		return ret;
	}

	ret = fts_ecc_check(buf, pram_app_size, pram_start_addr);
	if (ret < 0) {
		FTS_ERROR("pram ecc check fail");
		return ret;
	}

	FTS_INFO("pram app write successfully");
	return 0;
}

static int fts_dram_write_ecc(const u8 *buf, u32 len)
{
	int ret = 0;
	u32 dram_size = 0;
	u32 pram_app_size = 0;
	u32 dram_start_addr = 0;
	u16 const_len = 0;
	u16 const_len_n = 0;
	const u8 *dram_buf = NULL;

	FTS_INFO("begin to write dram data(bin len:%d)", len);
	pram_app_size = (((u16)buf[FTS_APP_INFO_OFFSET + 0] << 8) +
			 buf[FTS_APP_INFO_OFFSET + 1]) * 2;
	const_len = ((u16)buf[FTS_APP_INFO_OFFSET + 8] << 8) + buf[FTS_APP_INFO_OFFSET + 9];
	const_len_n = ((u16)buf[FTS_APP_INFO_OFFSET + 10] << 8) + buf[FTS_APP_INFO_OFFSET + 11];
	if ((const_len + const_len_n) != 0xFFFF) {
		FTS_INFO("no support dram,const len(%x %x)", const_len, const_len_n);
		return 0;
	}

	dram_size = ((u32)const_len) * 2;
	if ((pram_app_size + dram_size) > len) {
		FTS_ERROR("dram size(%d) > bin len(%d)", dram_size, len);
		return -EINVAL;
	}

	dram_buf = buf + pram_app_size;
	dram_start_addr = 0xD00000;
	ret = fts_dpram_write(dram_start_addr, dram_buf, dram_size, false);
	if (ret < 0) {
		FTS_ERROR("write dram fail");
		return ret;
	}

	ret = fts_ecc_check(dram_buf, dram_size, dram_start_addr);
	if (ret < 0) {
		FTS_ERROR("dram ecc check fail");
		return ret;
	}

	FTS_INFO("dram data write successfully");
	return 0;
}

static int fts_pram_start(void)
{
	int ret = 0;
	u8 cmd = FTS_CMD_START_APP;

	FTS_INFO("remap to start pram");
	ret = fts_write(&cmd, 1);
	if (ret < 0) {
		FTS_ERROR("write start pram cmd fail");
		return ret;
	}

	msleep(10);
	return 0;
}

static int fts_fw_write_start(const u8 *buf, u32 len, bool need_reset)
{
	int ret = 0;

	FTS_INFO("begin to write and start fw(bin len:%d)", len);
	if (need_reset) {
		ret = fts_enter_into_boot();
		if (ret < 0) {
			FTS_ERROR("enter into boot environment fail");
			return ret;
		}
	}

	ret = fts_pram_write_ecc(buf, len);
	if (ret < 0) {
		FTS_ERROR("write pram fail");
		return ret;
	}

	ret = fts_dram_write_ecc(buf, len);
	if (ret < 0) {
		FTS_ERROR("write dram fail");
		return ret;
	}

	ret = fts_pram_start();
	if (ret < 0) {
		FTS_ERROR("pram start fail");
		return ret;
	}

	fts_data->fw_is_running = 1;
	FTS_INFO("fw download and start successfully");
	return 0;
}

static int fts_fw_download(const u8 *buf, u32 len, bool need_reset)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;

	FTS_INFO("fw upgrade download function");
	if (!buf || (len < FTS_FW_BIN_LEN_MIN)) {
		FTS_ERROR("fw/len(%d) is invalid", len);
		return -EINVAL;
	}

	ts_data->fw_loading = true;
	fts_irq_disable();
	fts_esdcheck_switch(ts_data, false);

	for (i = 0; i < 3; i++) {
		FTS_INFO("fw download times:%d", i + 1);
		ret = fts_fw_write_start(buf, len, need_reset);
		if (ret == 0)
			break;
	}

	if (i >= 3) {
		FTS_ERROR("fw download fail");
		ret = -EIO;
	}

	fts_esdcheck_switch(ts_data, true);
	fts_irq_enable();
	ts_data->fw_loading = false;

	return ret;
}

static int fts_fw_resume(bool need_reset)
{
	int ret = 0;
	struct fts_upgrade *upg = fwupgrade;
	const struct firmware *fw = NULL;
	char fwname[128] = { 0 };
	const u8 *buf = NULL;
	u32 len = 0;

	FTS_INFO("fw upgrade resume function");
	if (!upg || !upg->fw) {
		FTS_ERROR("upg/fw is null");
		return -EINVAL;
	}

	if (upg->fw_from_request) {
		buf = upg->fw_from_request;
		len = upg->fw_from_request_len;
	} else {
		buf = upg->fw;
		len = upg->fw_length;
	}

	if (!buf || (len < FTS_FW_BIN_LEN_MIN)) {
		if (fts_noflash_controller.ts && fts_noflash_controller.ts->supplier) {
			snprintf(fwname, sizeof(fwname), "%s_0%d.bin",
				 "focaltech_ts_fw",
				 fts_noflash_controller.ts->supplier->id);
			ret = request_firmware(&fw, fwname, upg->ts_data->dev);
			if (ret == 0 && fw) {
				buf = fw->data;
				len = fw->size;
			}
		}
	}

	if (buf && (len >= FTS_FW_BIN_LEN_MIN))
		ret = fts_fw_download(buf, len, need_reset);
	else
		ret = -EINVAL;

	if (fw)
		release_firmware(fw);

	return ret;
}

int fts_upgrade_bin(char *fw_name, bool force)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	struct fts_ts_data *ts_data = fts_data;

	FTS_INFO("start upgrade with fw bin:%s", fw_name);
	if (!fw_name || !ts_data)
		return -EINVAL;

	ret = request_firmware(&fw, fw_name, ts_data->dev);
	if (ret || !fw) {
		FTS_ERROR("request_firmware(%s) fail, ret=%d", fw_name, ret);
		return ret ? ret : -ENOENT;
	}

	FTS_INFO("fw bin file len:%d", (int)fw->size);
	ret = fts_fw_download(fw->data, fw->size, true);
	release_firmware(fw);
	return ret;
}

int fts_enter_test_environment(bool test_state)
{
	FTS_INFO("fw test download function");
	return fts_fw_resume(true);
}

int fts_fw_recovery(void)
{
	int ret = 0;
	u8 boot_state = 0;
	u8 chip_id = 0;
	struct fts_upgrade *upg = fwupgrade;

	FTS_INFO("check if boot recovery");
	if (!upg || !upg->ts_data)
		return -EINVAL;

	if (fts_check_bootid() == 0) {
		FTS_INFO("check boot id pass, start download fw");
		ret = fts_fw_resume(false);
		if (ret < 0)
			FTS_ERROR("fw recovery fail");
		else
			FTS_INFO("boot recovery pass");
		return ret;
	}

	ret = fts_read_reg(FTS_REG_GESTURE_EN, &boot_state);
	if (ret < 0) {
		FTS_ERROR("read reg d0 fail, ret=%d", ret);
		return ret;
	}

	ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id);
	if ((ret >= 0) && (chip_id == upg->ts_data->ic_info.ids.chip_idh)) {
		fts_tp_state_recovery(upg->ts_data);
		return 0;
	}

	FTS_INFO("not in boot mode(0x%x),exit", boot_state);
	return -EIO;
}

int fts_enter_gesture_fw(void)
{
	return fts_fw_resume(true);
}

int fts_enter_normal_fw(void)
{
	return fts_fw_resume(true);
}

static void fts_fwupg_work(struct work_struct *work)
{
	int ret = 0;
	int retry = 0;
	const struct firmware *fw = NULL;
	char fwname[128] = { 0 };
	struct fts_upgrade *upg = fwupgrade;
	struct fts_ts_data *ts_data = fts_data;
	int supplier_id = 1;

	FTS_INFO("fw upgrade work function");
	if (!upg || !ts_data)
		return;

	FTS_DEBUG("get upgrade fw file");
	if (fts_noflash_controller.ts && fts_noflash_controller.ts->supplier)
		supplier_id = fts_noflash_controller.ts->supplier->id;

	snprintf(fwname, sizeof(fwname), "%s_0%d.bin", "focaltech_ts_fw", supplier_id);

	for (retry = 0; retry < 3; retry++) {
		ret = request_firmware(&fw, fwname, ts_data->dev);
		if (ret == 0 && fw)
			break;
		FTS_INFO("get fw file(default) abnormal");
		msleep(200);
	}

	if (ret || !fw) {
		FTS_ERROR("request_firmware(%s) fail", fwname);
		return;
	}

	upg->fw = vmalloc(fw->size);
	if (upg->fw) {
		memcpy(upg->fw, fw->data, fw->size);
		upg->fw_length = fw->size;
		upg->fw_from_file = true;
	}
	release_firmware(fw);

	if (upg->fw && upg->fw_length >= FTS_FW_BIN_LEN_MIN) {
		ret = fts_fw_download(upg->fw, upg->fw_length, true);
		if (ret == 0) {
			msleep(50);
			fts_read_reg(FTS_REG_FW_VER, &ts_data->fw_ver);
			FTS_INFO("fw upgrade success, fw ver:0x%02x", ts_data->fw_ver);
		}
	}
}

static void fts_initupg_work(struct work_struct *work)
{
	int ret = 0;
	int retry = 0;
	char ininame[128] = { 0 };
	struct fts_ts_data *ts_data = fts_data;
	int supplier_id = 1;

	if (!ts_data)
		return;

	if (fts_noflash_controller.ts && fts_noflash_controller.ts->supplier)
		supplier_id = fts_noflash_controller.ts->supplier->id;

	snprintf(ininame, sizeof(ininame), "%s_0%d.ini", "Conf_MultipleTest", supplier_id);

	for (retry = 0; retry < 3; retry++) {
		ret = request_firmware(&ts_data->ini_fw, ininame, ts_data->dev);
		if (ret == 0 && ts_data->ini_fw)
			break;
		mdelay(5);
	}

	if (ret)
		FTS_INFO("request_firmware(%s) fail, ret=%d", ininame, ret);
}

int fts_fwupg_init(struct fts_ts_data *ts_data)
{
	int i = 0;
	int setting_count = ARRAY_SIZE(upgrade_setting_list);
	struct upgrade_setting_nf *setting = &upgrade_setting_list[0];

	FTS_INFO("fw upgrade init function");
	if (!ts_data || !ts_data->ts_workqueue) {
		FTS_ERROR("ts_data/workqueue is NULL, can't run upgrade function");
		return -EINVAL;
	}

	fwupgrade = kzalloc(sizeof(*fwupgrade), GFP_KERNEL);
	if (!fwupgrade) {
		FTS_ERROR("malloc memory for upgrade fail");
		return -ENOMEM;
	}

	for (i = 0; i < setting_count; i++) {
		setting = &upgrade_setting_list[i];
		if ((setting->rom_idh == ts_data->ic_info.ids.rom_idh) &&
		    (setting->rom_idl == ts_data->ic_info.ids.rom_idl)) {
			FTS_INFO("match upgrade setting, type(ID):0x%02x%02x",
				 setting->rom_idh, setting->rom_idl);
			fwupgrade->setting_nf = setting;
			break;
		}
	}

	if (!fwupgrade->setting_nf) {
		FTS_ERROR("no upgrade settings match, can't upgrade");
		kfree(fwupgrade);
		fwupgrade = NULL;
		return -ENODATA;
	}

	fwupgrade->ts_data = ts_data;
	fwupgrade->module_info = &module_list[0];

	fts_esdcheck_switch(ts_data, false);
	INIT_WORK(&ts_data->fwupg_work, fts_fwupg_work);
	INIT_WORK(&ts_data->initupg_work, fts_initupg_work);
	queue_work(ts_data->ts_workqueue, &ts_data->fwupg_work);
	queue_work(ts_data->ts_workqueue, &ts_data->initupg_work);

	return 0;
}

int fts_fwupg_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	cancel_work_sync(&ts_data->fwupg_work);
	cancel_work_sync(&ts_data->initupg_work);
	if (ts_data->ini_fw) {
		release_firmware(ts_data->ini_fw);
		ts_data->ini_fw = NULL;
	}
	if (fwupgrade) {
		if (fwupgrade->fw_from_file && fwupgrade->fw) {
			vfree(fwupgrade->fw);
			fwupgrade->fw = NULL;
		}
		kfree(fwupgrade);
		fwupgrade = NULL;
	}
	FTS_FUNC_EXIT();
	return 0;
}
