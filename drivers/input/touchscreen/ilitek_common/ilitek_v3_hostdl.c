// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI Host-Download (IRAM) Firmware Upgrade
 * Supports Intel HEX (/vendor/firmware/ilitek_ts_fw_0X.bin) and .ili fallback.
 */
#include "ilitek_v3.h"
#include "ilitek_v3_fw.h"

#define UPDATE_RETRY_COUNT		3
#define MAX_HEX_FILE_SIZE_DL		(256 * 1024)
#define CRC_POLY			0x04C11DB7
#define ILI_FILE_HEADER_SIZE		64

/* Block tags in ILITEK HEX/ILI files */
#define BLOCK_TAG_AP			1
#define BLOCK_TAG_DATA			2
#define BLOCK_TAG_TUNING		3
#define BLOCK_TAG_GESTURE		4
#define BLOCK_TAG_MP			5

struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	int delay_after_upgrade;
	bool isCRC;
	bool isboot;
	bool is80k;
	int hex_tag;
};

struct flash_block_info {
	const char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
	u8 tag;
};

static u8 *pfw;
static u8 *CTPM_FW;
static struct touch_fw_data tfd;
static struct flash_block_info fbi[MAX_BLOCK_NUM];

static u32 ilitek_fw_calc_file_crc(u32 start, u32 end, u8 *data)
{
	u32 i, j;
	u32 crc = 0xFFFFFFFF;

	if (!data || end <= start)
		return 0;

	for (i = start; i < end; i++) {
		crc ^= ((u32)data[i] << 24);
		for (j = 0; j < 8; j++) {
			if (crc & 0x80000000)
				crc = (crc << 1) ^ CRC_POLY;
			else
				crc <<= 1;
		}
	}
	return crc;
}

static int ilitek_tddi_fw_iram_read(u8 *buf, u32 start, int len)
{
	int limit = 4096;
	int addr = 0, loop = 0, tmp_len = len, cnt = 0;
	u8 cmd[4] = { 0 };

	if (!buf || len <= 0)
		return -EINVAL;

	loop = (len + limit - 1) / limit;

	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		tmp_len = (len - cnt * limit) > limit ? limit : (len - cnt * limit);
		cmd[0] = 0x25;
		cmd[3] = (u8)((addr & 0x00FF0000) >> 16);
		cmd[2] = (u8)((addr & 0x0000FF00) >> 8);
		cmd[1] = (u8)((addr & 0x000000FF) >> 0);

		if (ilits->wrapper(cmd, sizeof(cmd), NULL, 0, false, false) < 0)
			return -EIO;
		if (ilits->wrapper(NULL, 0, buf + cnt * limit, tmp_len, false, false) < 0)
			return -EIO;
	}
	return 0;
}

int ili_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu)
{
	int ret = 0;
	int len = end - start + 1;
	u8 *buf;
	bool ice = atomic_read(&ilits->ice_stat);

	if (len <= 0 || len > MAX_IRAM_FIRMWARE_SIZE)
		return -EINVAL;

	buf = vmalloc(len);
	if (!buf)
		return -ENOMEM;

	if (!ice)
		ili_ice_mode_ctrl(ENABLE, mcu);

	ret = ilitek_tddi_fw_iram_read(buf, start, len);
	if (ret < 0)
		ILI_ERR("Read IRAM data failed\n");

	if (!ice)
		ili_ice_mode_ctrl(DISABLE, mcu);

	vfree(buf);
	return ret;
}

static int ilitek_tddi_fw_iram_write(u8 *data, u32 start, u32 len)
{
	int limit = 2048;
	int cnt, loop;
	u32 addr, chunk;
	u8 *txbuf;
	int ret = 0;

	txbuf = kzalloc(limit + 4, GFP_KERNEL);
	if (!txbuf)
		return -ENOMEM;

	loop = (len + limit - 1) / limit;
	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		chunk = (len - cnt * limit) > limit ? limit : (len - cnt * limit);
		txbuf[0] = 0x25;
		txbuf[1] = (u8)(addr & 0xFF);
		txbuf[2] = (u8)((addr >> 8) & 0xFF);
		txbuf[3] = (u8)((addr >> 16) & 0xFF);
		memcpy(txbuf + 4, data + cnt * limit, chunk);

		ret = ilits->wrapper(txbuf, chunk + 4, NULL, 0, false, false);
		if (ret < 0) {
			ILI_ERR("Write IRAM chunk failed at 0x%x\n", addr);
			break;
		}
	}

	kfree(txbuf);
	return ret;
}

static int hex_nibble(u8 c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex_byte(const u8 *p)
{
	int hi = hex_nibble(p[0]);
	int lo = hex_nibble(p[1]);

	if (hi < 0 || lo < 0)
		return -1;
	return (hi << 4) | lo;
}

static const char *block_tag_name(u8 tag)
{
	switch (tag) {
	case BLOCK_TAG_AP:
		return "AP";
	case BLOCK_TAG_DATA:
		return "DATA";
	case BLOCK_TAG_TUNING:
		return "TUNING";
	case BLOCK_TAG_GESTURE:
		return "GESTURE";
	case BLOCK_TAG_MP:
		return "MP";
	default:
		return "UNKNOWN";
	}
}

/* Parse ILITEK Intel HEX firmware image into pfw[] and fbi[] */
static int ilitek_tddi_fw_hex_convert(const u8 *hex, size_t hex_size)
{
	size_t pos = 0;
	u32 ext_addr = 0;
	int block_idx = 0;
	int i;

	tfd.block_number = 0;
	tfd.start_addr = 0xFFFFFFFF;
	tfd.end_addr = 0;

	while (pos < hex_size) {
		int len, addr_hi, addr_lo, type;
		u32 addr;
		u8 rec[64];

		/* Find start of record ':' */
		while (pos < hex_size && hex[pos] != ':')
			pos++;
		if (pos + 11 > hex_size)
			break;
		pos++; /* skip ':' */

		len = hex_byte(hex + pos);
		addr_hi = hex_byte(hex + pos + 2);
		addr_lo = hex_byte(hex + pos + 4);
		type = hex_byte(hex + pos + 6);
		if (len < 0 || addr_hi < 0 || addr_lo < 0 || type < 0)
			break;
		pos += 8;

		if (len > (int)sizeof(rec) || pos + (size_t)len * 2 + 2 > hex_size)
			break;

		for (i = 0; i < len; i++) {
			int b = hex_byte(hex + pos + i * 2);

			if (b < 0)
				return -EINVAL;
			rec[i] = (u8)b;
		}
		pos += (size_t)len * 2 + 2; /* skip data + checksum */

		addr = ext_addr | ((u32)addr_hi << 8) | (u32)addr_lo;

		switch (type) {
		case 0x00: /* Data record */
			if (addr + len <= MAX_HEX_FILE_SIZE_DL) {
				memcpy(pfw + addr, rec, len);
				if (addr < tfd.start_addr)
					tfd.start_addr = addr;
				if (addr + len > tfd.end_addr)
					tfd.end_addr = addr + len;
			}
			break;

		case 0x01: /* EOF */
			goto done;

		case 0x02: /* Extended segment address */
			if (len >= 2)
				ext_addr = (((u32)rec[0] << 8) | rec[1]) << 4;
			break;

		case 0x04: /* Extended linear address */
			if (len >= 2)
				ext_addr = (((u32)rec[0] << 8) | rec[1]) << 16;
			break;

		case 0xAF: /* ILITEK block start/end/tag */
			if (len >= 7 && block_idx < MAX_BLOCK_NUM) {
				u32 bstart = ((u32)rec[0] << 16) | ((u32)rec[1] << 8) | rec[2];
				u32 bend = ((u32)rec[3] << 16) | ((u32)rec[4] << 8) | rec[5];
				u8 tag = rec[6];

				fbi[block_idx].start = bstart;
				fbi[block_idx].end = bend;
				fbi[block_idx].len = (bend >= bstart) ? (bend - bstart + 1) : 0;
				fbi[block_idx].mem_start = bstart;
				fbi[block_idx].fix_mem_start = bstart;
				fbi[block_idx].tag = tag;
				fbi[block_idx].name = block_tag_name(tag);
				block_idx++;
			}
			break;

		case 0xB0: /* ILITEK block IRAM destination address */
			if (len >= 4) {
				u32 mem = ((u32)rec[0] << 16) | ((u32)rec[1] << 8) | rec[2];
				u8 tag = rec[3];

				for (i = 0; i < block_idx; i++) {
					if (fbi[i].tag == tag) {
						fbi[i].mem_start = mem;
						fbi[i].fix_mem_start = mem;
					}
				}
			}
			break;

		default:
			break;
		}
	}

done:
	tfd.block_number = block_idx;
	ILI_INFO("Parsed HEX: blocks=%d, start=0x%x, end=0x%x\n",
		 tfd.block_number, tfd.start_addr, tfd.end_addr);
	for (i = 0; i < block_idx; i++) {
		if (fbi[i].tag == BLOCK_TAG_GESTURE &&
		    fbi[i].mem_start == fbi[i].start &&
		    0x13FC8 <= MAX_HEX_FILE_SIZE_DL) {
			u32 ges_iram = (u32)pfw[0x13FC4] |
				       ((u32)pfw[0x13FC5] << 8) |
				       ((u32)pfw[0x13FC6] << 16) |
				       ((u32)pfw[0x13FC7] << 24);
			if (ges_iram > 0 && ges_iram < 0x14000) {
				u32 trim = fbi[i].len;

				while (trim > 4 && pfw[fbi[i].start + trim - 1] == 0xFF)
					trim--;
				trim = ALIGN(trim, 4);
				fbi[i].mem_start = ges_iram;
				fbi[i].len = trim;
			}
		}
		ILI_INFO("Block[%d] %s (tag=%d): hex 0x%x..0x%x (len=0x%x) -> IRAM 0x%x\n",
			 i, fbi[i].name, fbi[i].tag,
			 fbi[i].start, fbi[i].end, fbi[i].len, fbi[i].mem_start);
	}
	return (block_idx > 0 && tfd.end_addr > 0) ? 0 : -EINVAL;
}

/* Parse fallback .ili binary array (64-byte header + payload) */
static int ilitek_tddi_fw_ili_convert(const u8 *ili, size_t ili_size)
{
	int block_num, i;
	size_t payload_len;

	if (ili_size <= ILI_FILE_HEADER_SIZE)
		return -EINVAL;

	block_num = ili[33];
	if (block_num <= 0 || block_num > MAX_BLOCK_NUM)
		return -EINVAL;

	payload_len = min_t(size_t, ili_size - ILI_FILE_HEADER_SIZE, MAX_HEX_FILE_SIZE_DL);
	memcpy(pfw, ili + ILI_FILE_HEADER_SIZE, payload_len);

	tfd.block_number = block_num;
	tfd.start_addr = 0;
	tfd.end_addr = payload_len;

	for (i = 0; i < block_num; i++) {
		u32 bstart = ((u32)ili[34 + i * 6] << 16) |
			     ((u32)ili[35 + i * 6] << 8) |
			     ili[36 + i * 6];
		u32 bend = ((u32)ili[37 + i * 6] << 16) |
			   ((u32)ili[38 + i * 6] << 8) |
			   ili[39 + i * 6];

		fbi[i].start = bstart;
		fbi[i].end = bend;
		fbi[i].len = (bend >= bstart) ? (bend - bstart + 1) : 0;
		fbi[i].mem_start = bstart;
		fbi[i].fix_mem_start = bstart;
		fbi[i].tag = (u8)(i + 1);
		fbi[i].name = block_tag_name(fbi[i].tag);
	}

	ILI_INFO("Parsed ILI fallback: blocks=%d, payload=%zu\n", block_num, payload_len);
	return 0;
}

static int ilitek_tddi_fw_open_file(void)
{
	int ret;
	const struct firmware *fw = NULL;
	const char *name = ilits->fw_name[0] ? ilits->fw_name : "ilitek_ts_fw_00.bin";

	if (ilits->dev) {
		ret = request_firmware(&fw, name, ilits->dev);
		if (ret == 0 && fw && fw->size > 16) {
			ILI_INFO("Open firmware %s success, size = %zu\n", name, fw->size);
			if (fw->data[0] == ':')
				ret = ilitek_tddi_fw_hex_convert(fw->data, fw->size);
			else
				ret = ilitek_tddi_fw_ili_convert(fw->data, fw->size);
			release_firmware(fw);
			if (ret == 0)
				return 0;
		} else {
			if (fw)
				release_firmware(fw);
			ILI_ERR("Request firmware %s failed (%d), fallback to CTPM_FW_DEF\n",
				name, ret);
		}
	}

	CTPM_FW = CTPM_FW_DEF;
	return ilitek_tddi_fw_ili_convert(CTPM_FW, sizeof(CTPM_FW_DEF));
}

int ili_fw_upgrade(int op)
{
	int ret = 0, retry = UPDATE_RETRY_COUNT;
	int i;
	static bool get_firmware;
	u32 crc = 0;

	if (!ilits)
		return -ENODEV;

	if (!pfw) {
		pfw = vmalloc(MAX_HEX_FILE_SIZE_DL);
		if (!pfw) {
			ILI_ERR("Failed to allocate pfw memory\n");
			return -ENOMEM;
		}
	}

	if (!get_firmware || ilits->node_update) {
		memset(pfw, 0xFF, MAX_HEX_FILE_SIZE_DL);
		memset(fbi, 0, sizeof(fbi));
		memset(&tfd, 0, sizeof(tfd));

		ret = ilitek_tddi_fw_open_file();
		if (ret < 0) {
			ILI_ERR("Failed to open firmware file\n");
			return ret;
		}
		get_firmware = true;
	}

	/* Fast path: load ONLY gesture block into IRAM while AP firmware stays running */
	if (ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		ret = ili_ice_mode_ctrl(ENABLE, false);
		if (ret < 0) {
			ILI_ERR("Enable ICE mode failed for gesture code\n");
			return ret;
		}
		for (i = 0; i < tfd.block_number; i++) {
			if (fbi[i].tag != BLOCK_TAG_GESTURE)
				continue;
			if (fbi[i].len == 0 || fbi[i].start + fbi[i].len > MAX_HEX_FILE_SIZE_DL)
				continue;

			crc = ilitek_fw_calc_file_crc(fbi[i].start,
						      fbi[i].start + fbi[i].len - 4,
						      pfw);
			ILI_INFO("Download %s from hex 0x%x to IRAM 0x%x, len=0x%x, crc=0x%08x\n",
				 fbi[i].name, fbi[i].start, fbi[i].mem_start,
				 fbi[i].len, crc);
			ret = ilitek_tddi_fw_iram_write(pfw + fbi[i].start,
							fbi[i].mem_start,
							fbi[i].len);
			if (ret < 0)
				break;
		}
		ili_ice_mode_ctrl(DISABLE, false);
		return ret;
	}

	do {
		ili_irq_disable();
		ili_tp_reset();
		ret = ili_ice_mode_ctrl(ENABLE, false);
		if (ret < 0)
			continue;

		ili_wdt_reset_status(false, false);

		/* Download AP-mode blocks (AP, DATA, TUNING) to their IRAM addresses */
		for (i = 0; i < tfd.block_number; i++) {
			if (fbi[i].tag == BLOCK_TAG_GESTURE || fbi[i].tag == BLOCK_TAG_MP)
				continue;
			if (fbi[i].len == 0 || fbi[i].start + fbi[i].len > MAX_HEX_FILE_SIZE_DL)
				continue;

			crc = ilitek_fw_calc_file_crc(fbi[i].start,
						      fbi[i].start + fbi[i].len - 4,
						      pfw);
			ILI_INFO("Download %s from hex 0x%x to IRAM 0x%x, len=0x%x, crc=0x%08x\n",
				 fbi[i].name, fbi[i].start, fbi[i].mem_start,
				 fbi[i].len, crc);

			ret = ilitek_tddi_fw_iram_write(pfw + fbi[i].start,
							fbi[i].mem_start,
							fbi[i].len);
			if (ret < 0)
				break;
		}

		if (ret < 0) {
			ili_ice_mode_ctrl(DISABLE, false);
			continue;
		}

		/* Trigger IC code reset (0x40040 = 0xAE) then exit ICE mode to boot IRAM */
		ili_ic_code_reset(false);
		ili_ice_mode_ctrl(DISABLE, false);
		mdelay(20);

		/* Enable IRQ so cmd_int_check pulses wake up ilits->inq */
		ili_irq_enable();

		ret = ili_ic_get_all_info();
		if (ret == 0 && ilits->chip->fw_ver != 0) {
			ili_set_tp_data_len(DATA_FORMAT_DEMO, false, NULL);
			ilits->report = ENABLE;
			break;
		}
		ret = -EIO;
	} while (--retry > 0);

	if (ret == 0) {
		ilits->fw_base_ver = (ilits->chip->fw_ver >> 8) & 0xFF;
		ILI_INFO("Host download upgrade complete, fw_ver = 0x%08x\n",
			 ilits->chip->fw_ver);
	} else {
		ILI_ERR("Host download upgrade failed\n");
	}

	return ret;
}

/* Flash stub functions (host-download TDDI has no external flash) */
void ili_fw_read_flash_info(bool mcu)
{
}

int ilitek_tddi_flash_protect(bool enable, bool mcu)
{
	return 0;
}
