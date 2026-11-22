// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech SPI Bus Layer (SPI V2 protocol with CRC16 & status verification)
 */
#include "focaltech_core.h"

#define SPI_RETRY_NUMBER		3
#define CS_HIGH_DELAY			150
#define SPI_BUF_LENGTH			4096

#define DATA_CRC_EN			0x20
#define WRITE_CMD			0x00
#define READ_CMD			(0x80 | DATA_CRC_EN)

#define SPI_DUMMY_BYTE			3
#define SPI_HEADER_LENGTH		6   /* 4 cmd/len + 2 crc */

#define CD_PACKAGE_BUFLEN		4
#define MTK_SPI_PACKET_SIZE		1024

static void crckermit(u8 *data, u32 len, u16 *crc_out)
{
	u32 i, j;
	u16 crc = 0xFFFF;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if (crc & 0x0001)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc = crc >> 1;
		}
	}
	*crc_out = crc;
}

static int rdata_check(u8 *rdata, u32 rlen)
{
	u16 crc_calc = 0;
	u16 crc_read = 0;

	crckermit(rdata, rlen - 2, &crc_calc);
	crc_read = (u16)(rdata[rlen - 1] << 8) + rdata[rlen - 2];
	if (crc_calc != crc_read)
		return -EIO;

	return 0;
}

static int fts_spi_transfer(u8 *tx_buf, u8 *rx_buf, u32 len)
{
	int ret = 0;
	struct spi_device *spi = fts_data->spi;
	struct spi_message msg;
	struct spi_transfer xfer;

	memset(&xfer, 0, sizeof(xfer));
	spi_message_init(&msg);
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret) {
		FTS_ERROR("spi_sync fail,ret:%d", ret);
		return ret;
	}

	return 0;
}

static void fts_spi_buf_show(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	int size = 0;
	char *tmpbuf = NULL;

	if (!data || (datalen <= 0)) {
		FTS_ERROR("data/datalen is invalid");
		return;
	}

	size = (datalen > 256) ? 256 : datalen;
	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < size; i++)
		count += snprintf(tmpbuf + count, 1024 - count, "%02X ", data[i]);

	FTS_DEBUG("%s", tmpbuf);
	kfree(tmpbuf);
}

int fts_write(u8 *writebuf, u32 writelen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 *txbuf = NULL;
	u8 *rxbuf = NULL;
	u32 txlen = 0;
	u32 datalen = 0;
	u32 alloc_len = 0;

	if (!writebuf || (writelen == 0)) {
		FTS_ERROR("writebuf/writelen is invalid");
		return -EINVAL;
	}

	datalen = writelen - 1;
	alloc_len = writelen + 6 + ts_data->dummy_byte;

	mutex_lock(&ts_data->bus_lock);
	if (alloc_len > SPI_BUF_LENGTH) {
		txbuf = kzalloc(alloc_len, GFP_KERNEL);
		if (!txbuf) {
			FTS_ERROR("txbuf malloc fail");
			ret = -ENOMEM;
			goto err_out;
		}
		rxbuf = kzalloc(alloc_len, GFP_KERNEL);
		if (!rxbuf) {
			FTS_ERROR("rxbuf malloc fail");
			kfree(txbuf);
			ret = -ENOMEM;
			goto err_out;
		}
	} else {
		txbuf = ts_data->bus_tx_buf;
		rxbuf = ts_data->bus_rx_buf;
		memset(txbuf, 0x00, SPI_BUF_LENGTH);
		memset(rxbuf, 0x00, SPI_BUF_LENGTH);
	}

	txbuf[0] = writebuf[0];
	txbuf[1] = WRITE_CMD;
	txbuf[2] = (datalen >> 8) & 0xFF;
	txbuf[3] = datalen & 0xFF;
	if (datalen > 0) {
		memcpy(&txbuf[4 + SPI_DUMMY_BYTE], &writebuf[1], datalen);
		txlen = 4 + SPI_DUMMY_BYTE + datalen;
	} else {
		txlen = 4;
	}

	for (i = 0; i < SPI_RETRY_NUMBER; i++) {
		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if ((ret == 0) && ((rxbuf[3] & 0xA0) == 0))
			break;

		FTS_DEBUG("data write(addr:%x),status:%x,retry:%d,ret:%d",
			  writebuf[0], rxbuf[3], i, ret);
		ret = -EIO;
		udelay(CS_HIGH_DELAY);
	}

	if (ret < 0) {
		FTS_ERROR("data write(addr:%x) fail,status:%x,ret:%d",
			  writebuf[0], rxbuf[3], ret);
	}

	if (alloc_len > SPI_BUF_LENGTH) {
		kfree(txbuf);
		kfree(rxbuf);
	}

err_out:
	udelay(CS_HIGH_DELAY);
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_write_reg(u8 addr, u8 value)
{
	u8 writebuf[2] = { addr, value };

	return fts_write(writebuf, 2);
}

int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 *txbuf = NULL;
	u8 *rxbuf = NULL;
	u32 txlen = 0;
	u32 alloc_len = 0;
	u8 ctrl = READ_CMD;
	u32 dp = 0;

	if (!cmd || !cmdlen || !data || !datalen) {
		FTS_ERROR("cmd/cmdlen/data/datalen is invalid");
		return -EINVAL;
	}

	alloc_len = datalen + SPI_HEADER_LENGTH + SPI_DUMMY_BYTE + ts_data->dummy_byte;

	mutex_lock(&ts_data->bus_lock);
	if (alloc_len > SPI_BUF_LENGTH) {
		txbuf = kzalloc(alloc_len, GFP_KERNEL);
		if (!txbuf) {
			FTS_ERROR("txbuf malloc fail");
			ret = -ENOMEM;
			goto err_out;
		}
		rxbuf = kzalloc(alloc_len, GFP_KERNEL);
		if (!rxbuf) {
			FTS_ERROR("rxbuf malloc fail");
			kfree(txbuf);
			ret = -ENOMEM;
			goto err_out;
		}
	} else {
		txbuf = ts_data->bus_tx_buf;
		rxbuf = ts_data->bus_rx_buf;
		memset(txbuf, 0x00, SPI_BUF_LENGTH);
		memset(rxbuf, 0x00, SPI_BUF_LENGTH);
	}

	txbuf[0] = cmd[0];
	txbuf[1] = ctrl;
	txbuf[2] = (datalen >> 8) & 0xFF;
	txbuf[3] = datalen & 0xFF;
	dp = 4 + SPI_DUMMY_BYTE;
	txlen = dp + datalen + 2;

	for (i = 0; i < SPI_RETRY_NUMBER; i++) {
		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if ((ret == 0) && ((rxbuf[3] & 0xA0) == 0)) {
			memcpy(data, &rxbuf[dp], datalen);
			ret = rdata_check(&rxbuf[dp], datalen + 2);
			if (ret == 0)
				break;

			fts_spi_buf_show(&rxbuf[dp], datalen + 2);
			FTS_DEBUG("data read(addr:%x) crc abnormal,retry:%d",
				  cmd[0], i);
			ret = -EIO;
		} else {
			FTS_DEBUG("data read(addr:%x) status:%x,retry:%d,ret:%d",
				  cmd[0], rxbuf[3], i, ret);
			ret = -EIO;
		}
		udelay(CS_HIGH_DELAY);
	}

	if (ret < 0) {
		FTS_ERROR("data read(addr:%x) %s,status:%x,ret:%d",
			  cmd[0], (i >= SPI_RETRY_NUMBER) ? "crc abnormal" : "fail",
			  rxbuf[3], ret);
	}

	if (alloc_len > SPI_BUF_LENGTH) {
		kfree(txbuf);
		kfree(rxbuf);
	}

err_out:
	udelay(CS_HIGH_DELAY);
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_read_reg(u8 addr, u8 *value)
{
	return fts_read(&addr, 1, value, 1);
}

int fts_bus_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 *txbuf = NULL;
	u8 *rxbuf = NULL;
	u32 txlen = writelen;
	bool read_cmd = (readbuf && readlen);

	if (!writebuf || !writelen) {
		FTS_ERROR("writebuf/writelen is invalid");
		return -EINVAL;
	}

	if (read_cmd && (readlen > txlen))
		txlen = readlen;

	mutex_lock(&ts_data->bus_lock);
	if (txlen > SPI_BUF_LENGTH) {
		txbuf = kzalloc(txlen, GFP_KERNEL);
		if (!txbuf) {
			FTS_ERROR("txbuf malloc fail");
			ret = -ENOMEM;
			goto err_out;
		}
		rxbuf = kzalloc(txlen, GFP_KERNEL);
		if (!rxbuf) {
			FTS_ERROR("rxbuf malloc fail");
			kfree(txbuf);
			ret = -ENOMEM;
			goto err_out;
		}
	} else {
		txbuf = ts_data->bus_tx_buf;
		rxbuf = ts_data->bus_rx_buf;
		memset(txbuf, 0x00, SPI_BUF_LENGTH);
		memset(rxbuf, 0x00, SPI_BUF_LENGTH);
	}

	memcpy(txbuf, writebuf, writelen);
	ret = fts_spi_transfer(txbuf, rxbuf, txlen);
	if (ret == 0) {
		if (read_cmd)
			memcpy(readbuf, rxbuf, readlen);
	} else {
		FTS_ERROR("direct transfer(addr:%x) fail,status:%x,ret:%d",
			  writebuf[0], rxbuf[3], ret);
	}

	if (txlen > SPI_BUF_LENGTH) {
		kfree(txbuf);
		kfree(rxbuf);
	}

err_out:
	udelay(CS_HIGH_DELAY);
	mutex_unlock(&ts_data->bus_lock);
	return ret;
}

int fts_bus_configure(struct fts_ts_data *ts_data, u8 *buf, u32 size)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (size && buf && ts_data->spi) {
		ts_data->spi->mode = buf[0];
		ts_data->spi->bits_per_word = buf[1];
		ts_data->spi->max_speed_hz = *(u32 *)(buf + 3);
		FTS_INFO("spi,mode=%d,bits=%d,speed=%d",
			 ts_data->spi->mode,
			 ts_data->spi->bits_per_word,
			 ts_data->spi->max_speed_hz);
		ret = spi_setup(ts_data->spi);
		if (ret < 0)
			FTS_ERROR("spi setup fail,ret:%d", ret);
	}
	FTS_FUNC_EXIT();
	return ret;
}
