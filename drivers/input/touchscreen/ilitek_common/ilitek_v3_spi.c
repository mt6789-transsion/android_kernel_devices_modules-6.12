// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI SPI transport layer
 */
#include "ilitek_v3.h"

bool debug_en;
EXPORT_SYMBOL(debug_en);

static const u32 spi_clk_table[15] = {
	1000000,  2000000,  3000000,  4000000,  5000000,
	6000000,  7000000,  8000000,  9000000,  10000000,
	11000000, 12000000, 13000000, 14000000, 15000000,
};

static const struct of_device_id tp_match_table[] = {
	{ .compatible = "tchip,ilitek" },
	{},
};

static const struct spi_device_id tp_spi_id[] = {
	{ "mtk-tpd", 0 },
	{},
};

int ili_spi_write_then_read_split(struct spi_device *spi,
				  const void *txbuf, unsigned int n_tx,
				  void *rxbuf, unsigned int n_rx)
{
	int ret = 0;
	int duplex_len = 0;
	int xfercnt = 0;
	int xferlen = 0;
	int xferloop = 0;
	int offset = 0;
	u8 cmd = 0;
	struct spi_message message;
	struct spi_transfer xfer[4];

	if (!spi || !ilits || !ilits->spi_tx || !ilits->spi_rx)
		return -EINVAL;

	if (n_rx > SPI_TRANSFER_MAX_LEN) {
		ILI_ERR("Rx length (%u) exceeds limit (%u)\n", n_rx, SPI_TRANSFER_MAX_LEN);
		return -EINVAL;
	}

	memset(xfer, 0, sizeof(xfer));
	memset(ilits->spi_tx, 0, SPI_TRANSFER_MAX_LEN);
	memset(ilits->spi_rx, 0, SPI_TRANSFER_MAX_LEN);

	if (n_tx > 0 && n_rx > 0)
		cmd = SPI_READ;
	else
		cmd = SPI_WRITE;

	switch (cmd) {
	case SPI_WRITE:
		if (n_tx % SPI_TX_BUF_SIZE)
			xferloop = (n_tx / SPI_TX_BUF_SIZE) + 1;
		else
			xferloop = n_tx / SPI_TX_BUF_SIZE;

		xferlen = n_tx;
		memcpy(ilits->spi_tx, (u8 *)txbuf, xferlen);

		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > SPI_TX_BUF_SIZE)
				xferlen = SPI_TX_BUF_SIZE;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = ilits->spi_tx + xfercnt * SPI_TX_BUF_SIZE;
			if (xfercnt == 0)
				spi_message_init(&message);
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = n_tx - (xfercnt + 1) * SPI_TX_BUF_SIZE;
		}
		ret = spi_sync(spi, &message);
		break;

	case SPI_READ: {
		u32 dummy = atomic_read(&ilits->ice_stat) ? 0 : 2;

		if (n_tx > SPI_TX_BUF_SIZE) {
			ILI_ERR("Tx length (%u) exceeds limit\n", n_tx);
			return -EINVAL;
		}
		duplex_len = n_tx + n_rx + dummy;
		if (duplex_len % SPI_RX_BUF_SIZE)
			xferloop = (duplex_len / SPI_RX_BUF_SIZE) + 1;
		else
			xferloop = duplex_len / SPI_RX_BUF_SIZE;

		xferlen = duplex_len;
		memcpy(ilits->spi_tx, (u8 *)txbuf, n_tx);

		spi_message_init(&message);
		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > SPI_RX_BUF_SIZE)
				xferlen = SPI_RX_BUF_SIZE;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = ilits->spi_tx + offset;
			xfer[xfercnt].rx_buf = ilits->spi_rx + offset;
			spi_message_add_tail(&xfer[xfercnt], &message);
			offset += xferlen;
			xferlen = duplex_len - offset;
		}
		ret = spi_sync(spi, &message);
		if (ret == 0) {
			if (!atomic_read(&ilits->ice_stat) &&
			    ilits->spi_rx[1] != SPI_ACK) {
				ILI_ERR("SPI ACK error: 0x%x\n", ilits->spi_rx[1]);
				ret = -EIO;
			} else {
				memcpy((u8 *)rxbuf, ilits->spi_rx + dummy + 1, n_rx);
			}
		}
		break;
	}

	default:
		ILI_ERR("Unknown command 0x%x\n", cmd);
		ret = -EINVAL;
		break;
	}

	if (ret != 0)
		ILI_ERR("spi transfer failed, ret = %d\n", ret);

	return ret;
}

static int ili_spi_mp_precmd_check(u8 *txbuf, u32 wlen)
{
	if (!txbuf || wlen == 0)
		return 0;
	if (txbuf[0] == P5_X_SET_CDC_INIT && wlen < 127)
		return 1;
	return 0;
}

static int ili_spi_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen,
			   bool spi_irq, bool i2c_irq)
{
	int ret = 0;
	int mode = 0;
	u8 *wbuf = NULL;
	u32 write_len = 0;
	u32 header_len = 0;
	u8 dummy[2] = { SPI_WRITE, 0x00 };
	u8 rcmd = SPI_READ;
	u8 checksum;
	bool ice = atomic_read(&ilits->ice_stat) ? true : false;

	if (wlen > 0 && !txbuf) {
		ILI_ERR("txbuf is NULL\n");
		return -EINVAL;
	}

	if (rlen > 0 && !rxbuf) {
		ILI_ERR("rxbuf is NULL\n");
		return -EINVAL;
	}

	if (wlen > 0 && rlen > 0)
		mode = 2; /* write then read */
	else if (wlen > 0)
		mode = 1; /* write only */
	else if (rlen > 0)
		mode = 3; /* read only */
	else
		return 0;

	if (spi_irq)
		atomic_set(&ilits->cmd_int_check, ENABLE);

	/* Wake SPI PLL clock if needed */
	if (ilits->pll_clk_wakeup && !ice) {
		ILI_DBG("Write dummy to wake up spi pll clk\n");
		ilits->spi_write_then_read(ilits->spi, dummy, sizeof(dummy), NULL, 0);
		usleep_range(300, 400);
	}

	if (mode == 1 || mode == 2) {
		wbuf = kzalloc(wlen + 8, GFP_KERNEL);
		if (!wbuf) {
			ret = -ENOMEM;
			goto out;
		}

		wbuf[0] = SPI_WRITE;
		if (ice) {
			header_len = 1;
			memcpy(wbuf + 1, txbuf, wlen);
			write_len = wlen + 1;
		} else {
			header_len = 3;
			wbuf[1] = (u8)((wlen >> 8) & 0xFF);
			wbuf[2] = (u8)(wlen & 0xFF);
			memcpy(wbuf + 3, txbuf, wlen);
			write_len = wlen + 3;
			if (atomic_read(&ilits->mp_stat) &&
			    ili_spi_mp_precmd_check(txbuf, wlen)) {
				checksum = ili_calc_packet_checksum(txbuf, wlen);
				wbuf[write_len++] = checksum;
				wbuf[1] = (u8)(((write_len - header_len) >> 8) & 0xFF);
				wbuf[2] = (u8)((write_len - header_len) & 0xFF);
			}
		}

		ret = ilits->spi_write_then_read(ilits->spi, wbuf, write_len, NULL, 0);
		kfree(wbuf);
		if (!ice)
			udelay(250);
		if (ret < 0) {
			ILI_ERR("spi write failed, ret = %d\n", ret);
			goto out;
		}
	}

	if (mode == 2 || mode == 3) {
		if (mode == 2 && !ice && spi_irq && ilits->detect_int_stat) {
			if (ilits->detect_int_stat(false) < 0) {
				ILI_ERR("Wait INT stat failed\n");
				ret = -ETIMEDOUT;
				goto out;
			}
		}

		ret = ilits->spi_write_then_read(ilits->spi, &rcmd, 1, rxbuf, rlen);
		if (ret < 0) {
			ILI_ERR("spi read failed, ret = %d\n", ret);
			goto out;
		}
	}

out:
	if (spi_irq)
		atomic_set(&ilits->cmd_int_check, DISABLE);
	return ret;
}

int ili_core_spi_setup(int num)
{
	u32 freq;

	if (num < 0 || num >= ARRAY_SIZE(spi_clk_table)) {
		ILI_ERR("Invalid spi clk index %d, fallback to 7 (8MHz)\n", num);
		num = 7;
	}

	freq = spi_clk_table[num];
	ILI_INFO("spi clock = %u\n", freq);

	ilits->spi->mode = SPI_MODE_0;
	ilits->spi->bits_per_word = 8;
	ilits->spi->max_speed_hz = freq;

	if (spi_setup(ilits->spi) < 0) {
		ILI_ERR("Failed to setup spi device\n");
		return -ENODEV;
	}

	ILI_INFO("name = %s, bus_num = %d, cs = %d, mode = %d, speed = %d\n",
		 ilits->spi->modalias,
		 ilits->spi->controller->bus_num,
		 ilits->spi->chip_select[0],
		 ilits->spi->mode,
		 ilits->spi->max_speed_hz);

	return 0;
}

static struct ilitek_hwif_info *g_hwif;

static int ilitek_spi_probe(struct spi_device *spi)
{
	ILI_INFO("ilitek spi probe\n");

	if (!spi) {
		ILI_ERR("spi device is NULL\n");
		return -ENODEV;
	}

	ilits = devm_kzalloc(&spi->dev, sizeof(*ilits), GFP_KERNEL);
	if (!ilits) {
		ILI_ERR("Failed to allocate ilitek_ts_data\n");
		return -ENOMEM;
	}

	ilits->update_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL);
	if (!ilits->update_buf) {
		ILI_ERR("Failed to allocate update_buf\n");
		return -ENOMEM;
	}

	ilits->tr_buf = kzalloc(TR_BUF_SIZE, GFP_KERNEL);
	if (!ilits->tr_buf) {
		ILI_ERR("Failed to allocate tr_buf\n");
		return -ENOMEM;
	}

	ilits->spi_tx = kzalloc(SPI_TRANSFER_MAX_LEN, GFP_KERNEL);
	if (!ilits->spi_tx) {
		ILI_ERR("Failed to allocate spi_tx\n");
		return -ENOMEM;
	}

	ilits->spi_rx = kzalloc(SPI_TRANSFER_MAX_LEN, GFP_KERNEL);
	if (!ilits->spi_rx) {
		ILI_ERR("Failed to allocate spi_rx\n");
		return -ENOMEM;
	}

	ilits->spi = spi;
	ilits->dev = &spi->dev;
	ilits->hwif = g_hwif;
	ilits->spi_write_then_read = ili_spi_write_then_read_split;
	ilits->wrapper = ili_spi_wrapper;
	ilits->ice_mode_ctrl = ili_ice_mode_ctrl_by_mode_spi;
	ilits->detect_int_stat = ili_ic_check_int_pulse;
	ilits->int_mode = ili_ic_check_int_level;
	ilits->mp_move_code = ili_move_mp_code_iram;
	ilits->gesture_move_code = ili_move_gesture_code_iram;
	ilits->esd_recover = ili_touch_esd_gesture_iram;
	ilits->ges_recover = ili_touch_esd_gesture_iram;
	ilits->esd_func = ili_wq_esd_spi_check;

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ilits->tp_data_mode = DATA_FORMAT_DEMO;
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;
	ilits->rst_edge_delay = 5;
	ilits->wait_int_timeout = 2000;
	ilits->fw_open = 0;
	ilits->gesture = false;
	ilits->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ilits->int_pulse = true;
	ilits->pll_clk_wakeup = true;
	ilits->report = true;
	ilits->wq_ctrl = true;
	ilits->wq_esd_ctrl = false;
	ilits->wq_bat_ctrl = false;

	ili_core_spi_setup(7);

	if (ilits->hwif && ilits->hwif->plat_probe)
		return ilits->hwif->plat_probe();

	return ili_tddi_init();
}

static void ilitek_spi_remove(struct spi_device *spi)
{
	ILI_INFO("ilitek spi remove\n");
	if (ilits && ilits->hwif && ilits->hwif->plat_remove)
		ilits->hwif->plat_remove();
	else
		ili_dev_remove(true);
}

static struct spi_driver tp_spi_driver = {
	.driver = {
		.name = "mtk-tpd",
		.owner = THIS_MODULE,
		.of_match_table = tp_match_table,
	},
	.probe = ilitek_spi_probe,
	.remove = ilitek_spi_remove,
	.id_table = tp_spi_id,
};

int ili_interface_dev_init(struct ilitek_hwif_info *hwif)
{
	int ret;

	ILI_INFO("Register SPI driver\n");
	if (!hwif) {
		ILI_ERR("hwif is NULL\n");
		return -EINVAL;
	}

	g_hwif = hwif;
	tp_spi_driver.driver.name = hwif->name;
	tp_spi_driver.driver.owner = hwif->owner;
	tp_spi_driver.driver.of_match_table = hwif->of_match_table;

	ret = spi_register_driver(&tp_spi_driver);
	if (ret < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return ret;
	}

	return 0;
}

void ili_interface_dev_exit(struct ilitek_ts_data *ts, bool flag)
{
	ILI_INFO("Remove SPI driver\n");
	if (flag)
		spi_unregister_driver(&tp_spi_driver);

	if (!ts)
		return;

	kfree(ts->spi_tx);
	kfree(ts->spi_rx);
	kfree(ts->tr_buf);
	kfree(ts->update_buf);
	ts->spi_tx = NULL;
	ts->spi_rx = NULL;
	ts->tr_buf = NULL;
	ts->update_buf = NULL;
}
