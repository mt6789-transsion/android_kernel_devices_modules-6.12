// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI core state machine & workqueues
 */
#include "ilitek_v3.h"

struct ilitek_ts_data *ilits;

static struct workqueue_struct *esd_wq;
static struct delayed_work esd_work;
static struct workqueue_struct *bat_wq;
static struct delayed_work bat_work;
static struct workqueue_struct *resume_by_ddi_wq;
static struct work_struct resume_by_ddi_work;

static void ilitek_resume_by_ddi_work(struct work_struct *work)
{
	mutex_lock(&ilits->touch_mutex);

	if (ilits->gesture)
		disable_irq_wake(ilits->irq_num);

	/* Set tp as demo mode and reload code if it's iram. */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ilits->fw_upgrade_mode == 1)
		ili_fw_upgrade_handler(NULL);
	else
		ili_reset_ctrl(ilits->reset);

	ili_irq_enable();
	ILI_INFO("TP resume end by wq\n");
	ili_wq_ctrl(WQ_ESD, ENABLE);
	ili_wq_ctrl(WQ_BAT, ENABLE);
	ilits->tp_suspend = false;

	if (ili7807_controller.ts) {
		ILI_INFO("restore edge_palm = %d, game_mode = %d\n",
			 ili7807_controller.ts->edge_reject_switch,
			 ili7807_controller.ts->game_state);
		ili_ic_func_ctrl("edge_palm", ili7807_controller.ts->edge_reject_switch);
		ili_ic_func_ctrl("game_mode", ili7807_controller.ts->game_state ? 0 : 1);
	}

	mutex_unlock(&ilits->touch_mutex);
}

void ili_resume_by_ddi(void)
{
	if (!ilits)
		return;

	ILI_INFO("ili_resume_by_ddi start\n");
	ilitek_resume_by_ddi_work(NULL);
}

int ili_reset_ctrl(int mode)
{
	int ret = 0;

	atomic_set(&ilits->tp_reset, ENABLE);

	switch (mode) {
	case TP_IC_CODE_RST:
		ILI_INFO("TP IC Code RST\n");
		ret = ili_ic_code_reset(false);
		if (ret < 0)
			ILI_ERR("IC Code reset failed\n");
		break;
	case TP_IC_WHOLE_RST:
		ILI_INFO("TP IC whole RST without flash\n");
		ret = ili_ic_whole_reset(false, false);
		if (ret < 0)
			ILI_ERR("IC whole reset failed\n");
		break;
	case TP_HW_RST_ONLY:
		ILI_INFO("TP HW RST\n");
		ili_tp_reset();
		break;
	default:
		ILI_ERR("Unknown reset mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	/* Reset all variables */
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;
	ilits->pll_clk_wakeup = true;
	atomic_set(&ilits->tp_reset, DISABLE);
	return ret;
}

int ili_cascade_reset_ctrl(int reset_mode, bool enter_ice)
{
	int ret = 0;

	if (enter_ice) {
		ret = ili_ice_mode_ctrl_by_mode(ENABLE, false, BOTH);
		if (ret < 0)
			ILI_ERR("Enable ice mode failed before cascade reset\n");
	}
	ret = ili_reset_ctrl(reset_mode);
	return ret;
}

int ili_set_tp_data_len(int format, bool send, u8 *data)
{
	u8 cmd[10] = { 0 };
	u8 ctrl = 0;
	u16 self_key = 2;
	int ret = 0;
	int tp_mode = ilits->actual_tp_mode;
	int len = 0;

	if (tp_mode == P5_X_FW_GESTURE_MODE) {
		switch (format) {
		case DATA_FORMAT_GESTURE_NORMAL:
			len = P5_X_GESTURE_NORMAL_LENGTH;
			ctrl = DATA_FORMAT_GESTURE_NORMAL;
			break;
		case DATA_FORMAT_GESTURE_INFO:
			len = (ilits->rib.nReportResolutionMode == 1) ?
			      P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION :
			      P5_X_GESTURE_INFO_LENGTH;
			ctrl = DATA_FORMAT_GESTURE_INFO;
			break;
		case DATA_FORMAT_GESTURE_DEMO:
			len = (ilits->rib.nReportResolutionMode == 1) ? 72 : P5_X_DEMO_MODE_PACKET_LEN;
			ctrl = DATA_FORMAT_DEMO;
			break;
		default:
			ILI_ERR("Unknow gesture data format %d\n", format);
			return -EINVAL;
		}
	} else {
		switch (format) {
		case DATA_FORMAT_DEMO:
			len = (ilits->rib.nReportResolutionMode == 1) ? 72 : P5_X_DEMO_MODE_PACKET_LEN;
			ctrl = DATA_FORMAT_DEMO;
			break;
		case DATA_FORMAT_DEBUG:
			len = (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) +
			      (ilits->srx * 2) + (2 * self_key) + 35;
			ctrl = DATA_FORMAT_DEBUG;
			break;
		case DATA_FORMAT_DEMO_DEBUG_INFO:
			len = P5_X_DEMO_MODE_PACKET_LEN + P5_X_DEMO_DEBUG_INFO_ID0_LENGTH +
			      P5_X_INFO_HEADER_LENGTH;
			ctrl = DATA_FORMAT_DEMO_DEBUG_INFO;
			break;
		case DATA_FORMAT_DEBUG_LITE_ROI:
		case DATA_FORMAT_DEBUG_LITE_WINDOW:
		case DATA_FORMAT_DEBUG_LITE_AREA:
			len = P5_X_DEBUG_MODE_PACKET_LENGTH;
			ctrl = (u8)format;
			break;
		default:
			ILI_ERR("Unknow TP data format %d\n", format);
			return -EINVAL;
		}
	}

	if (len > TR_BUF_SIZE)
		len = TR_BUF_SIZE;

	ilits->tp_data_mode = format;
	ilits->tp_data_format = format;
	ilits->tp_data_len = len;
	ILI_INFO("TP mode = %d, format = %d, len = %d\n", tp_mode, format, len);

	if (tp_mode == P5_X_FW_GESTURE_MODE) {
		ili_set_gesture_symbol();
		if (send) {
			ret = ili_ic_func_ctrl("lpwg", ctrl);
			if (ret < 0)
				ILI_ERR("switch to gesture format %d failed\n", format);
		}
	} else if (send) {
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = (u8)tp_mode;
		cmd[2] = ctrl;
		if (data) {
			cmd[3] = data[0];
			ret = ilits->wrapper(cmd, 4, NULL, 0, true, false);
		} else {
			ret = ilits->wrapper(cmd, 3, NULL, 0, true, false);
		}
		if (ret < 0)
			ILI_ERR("switch to format %d failed\n", format);
	}

	return ret;
}

int ili_set_pen_data_len(u8 header, u8 ctrl, u8 type)
{
	u8 cmd[4] = { P5_X_MODE_CONTROL, header, ctrl, type };
	int ret;

	ilits->pen_mode = ctrl;
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, true, false);
	if (ret < 0)
		ILI_ERR("Set pen data len failed, ret = %d\n", ret);
	return ret;
}

int ili_switch_tp_mode(u8 mode)
{
	int ret = 0;
	bool ges_dbg = false;

	atomic_set(&ilits->tp_sw_mode, ENABLE);
	ilits->actual_tp_mode = mode;

	switch (ilits->actual_tp_mode) {
	case P5_X_FW_AP_MODE:
		ILI_INFO("Switch to AP mode\n");
		ilits->wait_int_timeout = 2000;
		if (ilits->fw_upgrade_mode == 1) {
			if (ili_reset_ctrl(ilits->reset) < 0)
				ILI_ERR("TP Reset failed during init\n");
			ret = ili_fw_upgrade_handler(NULL);
		} else {
			ret = ili_reset_ctrl(ilits->reset);
		}
		if (ret < 0)
			ILI_ERR("TP Reset failed during switch to AP mode\n");
		break;

	case P5_X_FW_GESTURE_MODE:
		ILI_INFO("Switch to Gesture mode\n");
		ilits->wait_int_timeout = 2000;
		if (ilits->gesture_move_code)
			ret = ilits->gesture_move_code(ilits->gesture_mode);
		if (ret < 0)
			ILI_ERR("Move gesture code failed\n");
		if (ges_dbg) {
			ILI_INFO("Enable gesture debug func\n");
			ili_set_tp_data_len(DATA_FORMAT_DEBUG, true, NULL);
		}
		break;

	case P5_X_FW_TEST_MODE:
		ILI_INFO("Switch to Test mode\n");
		ilits->wait_int_timeout = 6000;
		if (ilits->mp_move_code)
			ret = ilits->mp_move_code();
		break;

	default:
		ILI_ERR("Unknown TP mode: %x\n", mode);
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		ILI_ERR("Switch TP mode (%d) failed\n", mode);

	ILI_DBG("Actual TP mode = %d\n", ilits->actual_tp_mode);
	atomic_set(&ilits->tp_sw_mode, DISABLE);
	return ret;
}

int ili_fw_upgrade_handler(void *data)
{
	int ret = 0;
	struct tran_tpd_ops *tpd_ops = NULL;

	atomic_set(&ilits->fw_stat, ENABLE);
	ilits->fw_update_stat = 0;

	ret = ili_fw_upgrade(ilits->fw_open);
	if (ret != 0) {
		ILI_INFO("FW upgrade fail\n");
		ilits->fw_update_stat = -1;
	} else {
		ILI_INFO("FW upgrade pass\n");
		ilits->fw_update_stat = 100;
	}

	if (data && get_tpd_ops(&ili7807_controller, &tpd_ops) &&
	    tpd_ops->controller_init_finish)
		tpd_ops->controller_init_finish();

	atomic_set(&ilits->fw_stat, DISABLE);
	return ret;
}

void ili_wq_ctrl(int type, int ctrl)
{
	switch (type) {
	case WQ_ESD:
		if (ilits->wq_esd_ctrl) {
			if (!esd_wq) {
				ILI_ERR("wq esd is null\n");
				break;
			}
			ilits->wq_ctrl = (ctrl == ENABLE);
			if (ctrl == ENABLE) {
				ILI_DBG("execute esd check\n");
				queue_delayed_work(esd_wq, &esd_work, msecs_to_jiffies(2000));
			} else {
				cancel_delayed_work_sync(&esd_work);
				ILI_DBG("cancel esd check\n");
			}
		}
		break;
	case WQ_BAT:
		if (ilits->wq_bat_ctrl) {
			if (!bat_wq) {
				ILI_ERR("wq bat is null\n");
				break;
			}
			if (ctrl == ENABLE) {
				ILI_DBG("execute bat check\n");
				queue_delayed_work(bat_wq, &bat_work, msecs_to_jiffies(2000));
			} else {
				cancel_delayed_work_sync(&bat_work);
				ILI_DBG("cancel bat check\n");
			}
		}
		break;
	default:
		ILI_ERR("Unknown WQ type, %d\n", type);
		break;
	}
}

int ili_mp_test_handler(char *apk, bool lcm_on)
{
	int ret = 0;

	if (atomic_read(&ilits->fw_stat)) {
		ILI_ERR("fw upgrade processing, ignore\n");
		return -EMP_FW_PROC;
	}

	atomic_set(&ilits->mp_stat, ENABLE);

	if (ilits->actual_tp_mode != P5_X_FW_TEST_MODE) {
		ret = ili_switch_tp_mode(P5_X_FW_TEST_MODE);
		if (ret < 0) {
			ILI_ERR("Switch to Test mode failed\n");
			goto out;
		}
	}

	ret = ili_mp_test_main(apk, lcm_on);

out:
	/* Back to AP mode */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ilits->fw_upgrade_mode == 1) {
		if (ili_fw_upgrade_handler(NULL) < 0)
			ILI_ERR("FW upgrade failed after MP test\n");
	} else {
		ili_reset_ctrl(ilits->reset);
	}

	atomic_set(&ilits->mp_stat, DISABLE);
	return ret;
}

int ili_gesture_recovery(void)
{
	int ret = 0;

	atomic_set(&ilits->esd_stat, ENABLE);
	ILI_INFO("Doing gesture recovery\n");
	if (ilits->ges_recover)
		ret = ilits->ges_recover();
	atomic_set(&ilits->esd_stat, DISABLE);
	return ret;
}

int ili_spi_recovery(void)
{
	int ret = 0;

	atomic_set(&ilits->esd_stat, ENABLE);
	ILI_INFO("Doing spi recovery\n");
	if (ili_fw_upgrade_handler(NULL) < 0) {
		ILI_ERR("FW upgrade failed during spi recovery\n");
		ret = -EIO;
	}
	atomic_set(&ilits->esd_stat, DISABLE);
	return ret;
}

int ili_wq_esd_spi_check(void)
{
	int ret = 0;
	u8 tx = SPI_WRITE;
	u8 rx = 0;

	ret = ilits->spi_write_then_read(ilits->spi, &tx, 1, &rx, 1);
	ILI_DBG("spi esd check = 0x%x\n", rx);
	if (ret == 0 && rx != SPI_ACK) {
		ILI_ERR("SPI ACK failed, doing spi recovery\n");
		return -EIO;
	}
	return 0;
}

int ili_wq_esd_i2c_check(void)
{
	ILI_DBG("i2c esd check\n");
	return 0;
}

static void ilitek_tddi_wq_esd_check(struct work_struct *work)
{
	if (!ilits->wq_esd_ctrl)
		return;

	mutex_lock(&ilits->touch_mutex);
	if (ilits->esd_func && ilits->esd_func() < 0) {
		ili_irq_disable();
		if (ilits->esd_recover)
			ilits->esd_recover();
		ili_irq_enable();
	}
	mutex_unlock(&ilits->touch_mutex);

	if (ilits->wq_esd_ctrl)
		queue_delayed_work(esd_wq, &esd_work, msecs_to_jiffies(2000));
}

static void ilitek_tddi_wq_bat_check(struct work_struct *work)
{
	static int charge_mode;

	if (!ilits->wq_bat_ctrl)
		return;

	if (ili7807_controller.ts) {
		int cur = ili7807_controller.ts->usb_in ? 1 : 0;

		if (cur != charge_mode) {
			charge_mode = cur;
			mutex_lock(&ilits->touch_mutex);
			ili_ic_func_ctrl("plug", cur ? 0 : 1);
			mutex_unlock(&ilits->touch_mutex);
		}
	}

	if (ilits->wq_bat_ctrl)
		queue_delayed_work(bat_wq, &bat_work, msecs_to_jiffies(2000));
}

int ili_sleep_handler(int mode)
{
	int ret = 0;
	bool sense_stop = (ilits->chip && ilits->chip->core_ver < 0x01040300);
	struct tran_tpd_ops *tpd_ops = NULL;

	mutex_lock(&ilits->touch_mutex);
	atomic_set(&ilits->tp_sleep, ENABLE);

	if (atomic_read(&ilits->fw_stat) || atomic_read(&ilits->mp_stat)) {
		ILI_INFO("fw upgrade or mp still running, ignore sleep requst\n");
		atomic_set(&ilits->tp_sleep, DISABLE);
		mutex_unlock(&ilits->touch_mutex);
		return 0;
	}

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);
	ili_irq_disable();

	ILI_INFO("Sleep Mode = %d\n", mode);

	if (get_tpd_ops(&ili7807_controller, &tpd_ops) && tpd_ops->get_gesture_status)
		ilits->gesture = tpd_ops->get_gesture_status() ? true : false;

	switch (mode) {
	case TP_SUSPEND:
		ILI_INFO("TP suspend start\n");
		ilits->tp_suspend = true;
		if (ilits->fw_update_stat == 100) {
			if (ili_ic_func_ctrl("sense", DISABLE) < 0)
				ILI_ERR("Write sense stop cmd failed\n");
			else if (ili_ic_check_busy(50, 20, false) < 0)
				ILI_ERR("Check busy timeout during suspend\n");

			if (ilits->gesture) {
				ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
				enable_irq_wake(ilits->irq_num);
				ili_irq_enable();
			} else {
				if (ili_ic_func_ctrl("sleep", 0x00) < 0)
					ILI_ERR("Write sleep in cmd failed\n");
			}
		}
		ILI_INFO("TP normal suspend end\n");
		break;

	case TP_DEEP_SLEEP:
		ILI_INFO("TP deep suspend start\n");
		ilits->tp_suspend = true;
		if (ilits->fw_update_stat == 100) {
			if (sense_stop) {
				if (ili_ic_func_ctrl("sense", DISABLE) < 0)
					ILI_ERR("Write sense stop cmd failed\n");
				else if (ili_ic_check_busy(50, 20, false) < 0)
					ILI_ERR("Check busy timeout during deep suspend\n");
			}

			if (ilits->gesture) {
				ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
				enable_irq_wake(ilits->irq_num);
				ili_irq_enable();
			} else {
				if (ili_ic_func_ctrl("sleep", 0x01) < 0)
					ILI_ERR("Write deep sleep in cmd failed\n");
			}
		}
		ILI_INFO("TP deep suspend end\n");
		break;

	case TP_RESUME:
		ILI_INFO("TP resume start\n");
		if (ilits->gesture)
			disable_irq_wake(ilits->irq_num);

		ilits->actual_tp_mode = P5_X_FW_AP_MODE;
		if (ilits->fw_upgrade_mode == 1)
			ili_fw_upgrade_handler(NULL);
		else
			ili_reset_ctrl(ilits->reset);

		ili_irq_enable();
		ili_wq_ctrl(WQ_ESD, ENABLE);
		ili_wq_ctrl(WQ_BAT, ENABLE);
		ilits->tp_suspend = false;
		ILI_INFO("TP resume end\n");
		break;

	default:
		ILI_ERR("Unknown sleep mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	ili_touch_release_all_point();
	atomic_set(&ilits->tp_sleep, DISABLE);
	mutex_unlock(&ilits->touch_mutex);
	return ret;
}

void ili_report_handler(void)
{
	int ret = 0;
	int pid = 0;
	u8 checksum = 0;
	u8 pack_checksum = 0;
	u8 *trdata = ilits->tr_buf;
	int rlen = ilits->tp_data_len;

	if (!trdata || rlen <= 0 || rlen > TR_BUF_SIZE)
		return;

	memset(trdata, 0, rlen);

	if (ilits->pm_suspend) {
		if (!wait_for_completion_timeout(&ilits->pm_completion,
						 msecs_to_jiffies(700))) {
			ILI_ERR("System is busy, ignore report\n");
			return;
		}
	}

	if (ilits->wrapper(NULL, 0, trdata, rlen, true, false) < 0) {
		ILI_ERR("Read report data failed\n");
		return;
	}

	pid = trdata[0];
	ILI_DBG("Packet ID = 0x%x, len = %d\n", pid, rlen);

	if (pid == P5_X_I2CUART_PACKET_ID) {
		ili_report_i2cuart_mode(trdata, rlen);
		return;
	}

	if (pid == P5_X_GESTURE_PACKET_ID) {
		if (ilits->prox_face_mode)
			ili_report_proximity_mode(trdata, rlen);
		else
			ili_report_gesture_mode(trdata, rlen);
		return;
	}

	checksum = ili_calc_packet_checksum(trdata, rlen - 1);
	pack_checksum = trdata[rlen - 1];
	if (checksum != pack_checksum) {
		ILI_ERR("Checksum error: calc=0x%x, pack=0x%x, pid=0x%x\n",
			checksum, pack_checksum, pid);
		ili_dump_data(trdata, 8, rlen, 0, "err packet");
		return;
	}

	switch (pid) {
	case P5_X_DEMO_PACKET_ID:
	case P5_X_DEMO_PACKET_ID_HIGH_RES:
		ili_report_ap_mode(trdata, rlen);
		break;
	case P5_X_DEBUG_PACKET_ID:
		ili_report_debug_mode(trdata, rlen);
		break;
	case P5_X_DEMO_DEBUG_INFO_PACKET_ID:
		ili_demo_debug_info_mode(trdata, rlen);
		break;
	default:
		ILI_ERR("Unknown packet id = 0x%x\n", pid);
		break;
	}

	(void)ret;
}

int ili_tddi_init(void)
{
	int ret = 0;

	ILI_INFO("driver version = %s\n", DRIVER_VERSION);

	mutex_init(&ilits->touch_mutex);
	mutex_init(&ilits->debug_mutex);
	mutex_init(&ilits->debug_read_mutex);
	spin_lock_init(&ilits->irq_spin);
	init_waitqueue_head(&ilits->inq);
	init_completion(&ilits->pm_completion);

	/* Note: irq_stat was already set to ENABLE by ili_irq_register() */
	atomic_set(&ilits->ice_stat, DISABLE);
	atomic_set(&ilits->tp_reset, DISABLE);
	atomic_set(&ilits->fw_stat, DISABLE);
	atomic_set(&ilits->mp_stat, DISABLE);
	atomic_set(&ilits->tp_sleep, DISABLE);
	atomic_set(&ilits->cmd_int_check, DISABLE);
	atomic_set(&ilits->esd_stat, DISABLE);

	esd_wq = alloc_workqueue("esd_check", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	bat_wq = alloc_workqueue("bat_check", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (esd_wq)
		INIT_DELAYED_WORK(&esd_work, ilitek_tddi_wq_esd_check);
	if (bat_wq)
		INIT_DELAYED_WORK(&bat_work, ilitek_tddi_wq_bat_check);

	resume_by_ddi_wq = alloc_workqueue("resume_by_ddi_wq",
					   WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (resume_by_ddi_wq)
		INIT_WORK(&resume_by_ddi_work, ilitek_resume_by_ddi_work);

	ilits->ws = wakeup_source_register(ilits->dev, "ili_wakelock");
	ilits->demo_debug_info[0] = ili_demo_debug_info_id0;
	ilits->fw_upgrade_mode = 1; /* Host download IRAM */
	ilits->reset = TP_IC_WHOLE_RST;

	if (ili7807_controller.ts) {
		ili7807_controller.ts->edge_reject_switch = 0;
		if (ili7807_controller.ts->supplier) {
			snprintf(ilits->fw_name, sizeof(ilits->fw_name), "ilitek_ts_fw_0%d.bin",
				 ili7807_controller.ts->supplier->id);
			snprintf(ilits->ini_name, sizeof(ilits->ini_name), "Conf_MultipleTest_0%d.ini",
				 ili7807_controller.ts->supplier->id);
			ILI_INFO("fw_name = %s, ini_name = %s\n",
				 ilits->fw_name, ilits->ini_name);
		}
	}

	ili_ic_init();

	ili_tp_reset();
	ret = ili_ice_mode_ctrl(ENABLE, false);
	if (ret < 0) {
		ILI_ERR("Failed to enable ice mode during init\n");
		return ret;
	}

	if (ili_ic_get_info() < 0)
		ILI_ERR("Chip info is incorrect\n");

	ili_ice_mode_ctrl(DISABLE, false);

	ili_node_init();
	ili_input_register();

	ilits->update_thread = kthread_run(ili_fw_upgrade_handler, (void *)1,
					   "ili_fw_upgrade");
	/* Match stock check (which references ERR_PTR symbol) */
	if (ilits->update_thread == (void *)ERR_PTR || IS_ERR(ilits->update_thread)) {
		ILI_ERR("Failed to create fw upgrade thread\n");
		ilits->update_thread = NULL;
	}

	return 0;
}

int ili_dev_init(struct ilitek_hwif_info *hwif)
{
	ILI_INFO("TP Interface: SPI\n");
	return ili_interface_dev_init(hwif);
}

void ili_dev_remove(bool flag)
{
	ILI_INFO("remove ilitek dev\n");

	if (!ilits)
		return;

	ili_wq_ctrl(WQ_ESD, DISABLE);
	ili_wq_ctrl(WQ_BAT, DISABLE);

	if (esd_wq) {
		destroy_workqueue(esd_wq);
		esd_wq = NULL;
	}
	if (bat_wq) {
		destroy_workqueue(bat_wq);
		bat_wq = NULL;
	}
	if (resume_by_ddi_wq) {
		flush_workqueue(resume_by_ddi_wq);
		destroy_workqueue(resume_by_ddi_wq);
		resume_by_ddi_wq = NULL;
	}

	if (ilits->ws) {
		wakeup_source_unregister(ilits->ws);
		ilits->ws = NULL;
	}

	if (ilits->input) {
		input_unregister_device(ilits->input);
		ilits->input = NULL;
	}

	if (ilits->input_pen) {
		input_unregister_device(ilits->input_pen);
		ilits->input_pen = NULL;
	}

	if (gpio_is_valid(ilits->tp_int))
		gpio_free(ilits->tp_int);
	if (gpio_is_valid(ilits->tp_rst))
		gpio_free(ilits->tp_rst);

	ili_interface_dev_exit(ilits, flag);
}
