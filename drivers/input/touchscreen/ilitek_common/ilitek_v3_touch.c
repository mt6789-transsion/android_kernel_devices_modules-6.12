// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI touch, pen, gesture, proximity, and debug packet reporting
 */
#include "ilitek_v3.h"

static struct ilitek_touch_info touch_info[MAX_TOUCH_NUM];

u8 ili_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8)((-sum) & 0xFF);
}

int ili_touch_esd_gesture_flash(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;

	if (ili_ice_mode_ctrl(ENABLE, false) < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		return -EIO;
	}

	while (retry-- > 0) {
		if (ili_ice_mode_read(0x25FF8, &answer, sizeof(u32)) < 0)
			break;
		if (answer == 0x5AA5A55A)
			break;
		mdelay(5);
	}

	ili_ice_mode_ctrl(DISABLE, false);
	if (ili_move_gesture_code_flash(ilits->gesture_mode) < 0) {
		ILI_ERR("Move gesture code failed during gesture recovery\n");
		ret = -EIO;
	}
	return ret;
}

int ili_touch_esd_gesture_iram(void)
{
	int ret = 0;

	ILI_INFO("Doing esd gesture recovery (iram)\n");
	if (ili_fw_upgrade_handler(NULL) < 0) {
		ILI_ERR("FW upgrade failed during gesture recovery\n");
		return -EIO;
	}

	if (ilits->tp_suspend && ilits->gesture) {
		ret = ili_move_gesture_code_iram(ilits->gesture_mode);
		if (ret < 0)
			ILI_ERR("Move gesture code failed during gesture recovery\n");
	}

	return ret;
}

void ili_demo_debug_info_id0(u8 *buf, size_t len)
{
	if (!buf || len < 14)
		return;
	ILI_DBG("id = %d\n", buf[0]);
	ILI_DBG("app_sys_state_e = %d\n", buf[1]);
	ILI_DBG("algo_pt_status1 = %d\n", buf[2]);
	ILI_DBG("hopping_flg = %d\n", buf[3]);
}

void ili_demo_debug_info_mode(u8 *buf, size_t len)
{
	u8 *info_ptr;
	u8 info_id, info_len;

	ili_report_ap_mode(buf, P5_X_DEMO_MODE_PACKET_LEN);
	if (len <= P5_X_DEMO_MODE_PACKET_LEN + 2)
		return;

	info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN;
	info_len = info_ptr[0];
	info_id = info_ptr[1];
	if (info_id < ARRAY_SIZE(ilits->demo_debug_info) &&
	    ilits->demo_debug_info[info_id])
		ilits->demo_debug_info[info_id](info_ptr + 2, info_len);
}

static int ilitek_get_resolution_scale(void)
{
	if (ili7807_controller.ts && ili7807_controller.ts->supplier &&
	    ili7807_controller.ts->supplier->resolution > 0)
		return ili7807_controller.ts->supplier->resolution;
	return 1;
}

void ili_touch_press(u16 x, u16 y, u16 pressure, u16 id)
{
	int res = ilits->trans_xy ? 1 : ilitek_get_resolution_scale();

	if (!ilits->input)
		return;

	ILI_DBG("Touch Press: id = %d, x = %d, y = %d, p = %d\n", id, x, y, pressure);
	input_mt_slot(ilits->input, id);
	input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, true);
	input_report_abs(ilits->input, ABS_MT_POSITION_X, x * res);
	input_report_abs(ilits->input, ABS_MT_POSITION_Y, y * res);
	input_report_abs(ilits->input, ABS_MT_TOUCH_MAJOR, pressure ? pressure : 1);
	input_report_abs(ilits->input, ABS_MT_PRESSURE, pressure ? pressure : 1);
}

void ili_touch_release(u16 x, u16 y, u16 id)
{
	if (!ilits->input)
		return;

	ILI_DBG("Touch Release: id = %d, x = %d, y = %d\n", id, x, y);
	input_mt_slot(ilits->input, id);
	input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, false);
}

void ili_touch_release_all_point(void)
{
	int i;

	if (!ilits->input)
		return;

	ilits->finger = 0;
	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		ilits->curt_touch[i] = 0;
		ilits->prev_touch[i] = 0;
		ili_touch_release(0, 0, i);
	}
	input_report_key(ilits->input, BTN_TOUCH, 0);
	input_sync(ilits->input);
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
	mutex_lock(&ilits->debug_mutex);

	if (!ilits->dnp && !ilits->dbf)
		goto out;

	if (ilits->dbl && ilits->dbf_in_idx < TR_BUF_LIST_SIZE) {
		if (ilits->dbl[ilits->dbf_in_idx].data && len <= TR_BUF_SIZE) {
			memcpy(ilits->dbl[ilits->dbf_in_idx].data, buf, len);
			ilits->dbl[ilits->dbf_in_idx].mark = true;
			ilits->dbf_in_idx++;
			if (ilits->dbf_in_idx >= TR_BUF_LIST_SIZE) {
				ilits->dbf_in_idx = 0;
				ilits->all_frame_full = true;
			}
		}
	}

	wake_up(&ilits->inq);
out:
	mutex_unlock(&ilits->debug_mutex);
}

void ili_report_ap_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	u16 pwid = ilits->panel_wid ? ilits->panel_wid : TOUCH_SCREEN_X_MAX;
	u16 phei = ilits->panel_hei ? ilits->panel_hei : TOUCH_SCREEN_Y_MAX;
	u16 max_x = ilits->max_x ? ilits->max_x : TPD_WIDTH;
	u16 max_y = ilits->max_y ? ilits->max_y : TPD_WIDTH;

	memset(touch_info, 0, sizeof(touch_info));
	ilits->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if (ilits->rib.nReportResolutionMode == 1) {
			int off = 4 + 5 * i;

			if ((buf[off] == 0xFF) && (buf[off + 1] == 0xFF) &&
			    (buf[off + 2] == 0xFF) && (buf[off + 3] == 0xFF)) {
				ilits->curt_touch[i] = 0;
				continue;
			}

			xop = (buf[off] << 8) | buf[off + 1];
			yop = (buf[off + 2] << 8) | buf[off + 3];

			if (ilits->trans_xy) {
				touch_info[ilits->finger].x = xop;
				touch_info[ilits->finger].y = yop;
			} else {
				touch_info[ilits->finger].x = (xop * pwid) / max_x;
				touch_info[ilits->finger].y = (yop * phei) / max_y;
			}
			touch_info[ilits->finger].pressure = buf[off + 4];
		} else {
			if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF) &&
			    (buf[(4 * i) + 3] == 0xFF)) {
				ilits->curt_touch[i] = 0;
				continue;
			}

			xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
			yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));

			if (ilits->trans_xy) {
				touch_info[ilits->finger].x = xop;
				touch_info[ilits->finger].y = yop;
			} else {
				touch_info[ilits->finger].x = (xop * pwid) / TPD_WIDTH;
				touch_info[ilits->finger].y = (yop * phei) / TPD_WIDTH;
			}
			touch_info[ilits->finger].pressure = buf[(4 * i) + 4];
		}

		touch_info[ilits->finger].id = i;
		ilits->finger++;
		ilits->curt_touch[i] = 1;
	}

	if (ilits->finger) {
		input_report_key(ilits->input, BTN_TOUCH, 1);
		for (i = 0; i < ilits->finger; i++) {
			ili_touch_press(touch_info[i].x, touch_info[i].y,
					touch_info[i].pressure, touch_info[i].id);
		}
		for (i = 0; i < MAX_TOUCH_NUM; i++) {
			if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
				ili_touch_release(0, 0, i);
			ilits->prev_touch[i] = ilits->curt_touch[i];
		}
	} else {
		if (ilits->last_touch)
			ili_touch_release_all_point();
	}

	ilits->last_touch = ilits->finger;
	if (ilits->input)
		input_sync(ilits->input);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_pen_demo_mode_report_point(u8 *buf, int len)
{
	if (!ilits->input_pen || !buf || len < 12)
		return;

	ilits->pen_info.x = (buf[2] << 8) | buf[3];
	ilits->pen_info.y = (buf[4] << 8) | buf[5];
	ilits->pen_info.pressure = (buf[6] << 8) | buf[7];
	ilits->pen_info.tip_sw = (buf[1] & BIT(0)) ? true : false;

	input_report_key(ilits->input_pen, BTN_TOUCH, ilits->pen_info.tip_sw);
	input_report_key(ilits->input_pen, BTN_TOOL_PEN, ilits->pen_info.tip_sw);
	input_report_abs(ilits->input_pen, ABS_X, ilits->pen_info.x);
	input_report_abs(ilits->input_pen, ABS_Y, ilits->pen_info.y);
	input_report_abs(ilits->input_pen, ABS_PRESSURE, ilits->pen_info.pressure);
	input_sync(ilits->input_pen);
}

void ili_report_pen_ap_mode(u8 *buf, int len)
{
	ili_pen_demo_mode_report_point(buf, len);
}

void ili_pen_debug_mode_report_point(u8 *buf, int len)
{
	ili_pen_demo_mode_report_point(buf, len);
}

void ili_debug_mode_report_point(u8 *buf, int len)
{
	if (!buf || len < P5_X_DEMO_MODE_PACKET_LEN)
		return;
	ili_report_ap_mode(buf + 4, P5_X_DEMO_MODE_PACKET_LEN);
}

void ili_report_debug_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf, len);
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_pen_debug_mode(u8 *buf, int len)
{
	ili_pen_debug_mode_report_point(buf, len);
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_debug_lite_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf, len);
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_proximity_mode(u8 *buf, int len)
{
	struct tran_tpd_ops *tpd_ops = NULL;
	u8 prox_val;

	if (!buf || len < 2)
		return;

	prox_val = buf[1];
	ILI_INFO("proximity value = 0x%x\n", prox_val);

	if (get_tpd_ops(&ili7807_controller, &tpd_ops) &&
	    tpd_ops->report_proximity_event) {
		if (prox_val == 0x01)
			tpd_ops->report_proximity_event(TRAN_PS_STATE_NEAR);
		else
			tpd_ops->report_proximity_event(TRAN_PS_STATE_FAR);
	}
}

void ili_report_gesture_mode(u8 *buf, int len)
{
	struct tran_tpd_ops *tpd_ops = NULL;
	u8 gc;
	int key = 0;

	if (!buf || len < 2)
		return;

	gc = buf[1];
	ILI_INFO("gesture code = 0x%x\n", gc);

	if (ilits->ws)
		__pm_wakeup_event(ilits->ws, 1000);

	if (get_tpd_ops(&ili7807_controller, &tpd_ops) &&
	    tpd_ops->gesture_id_translate) {
		key = tpd_ops->gesture_id_translate(gc);
		ILI_INFO("gesture_id_translate(0x%x) = %d\n", gc, key);
	}

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_i2cuart_mode(u8 *buf, int len)
{
	u8 *uart_buf;

	if (!buf || len <= 0)
		return;

	uart_buf = kzalloc(len, GFP_KERNEL);
	if (!uart_buf) {
		ILI_ERR("Failed to allocate uart_buf memory %ld\n", (long)len);
		return;
	}

	memcpy(uart_buf, buf, len);
	ilitek_tddi_touch_send_debug_data(uart_buf, len);
	kfree(uart_buf);
}
