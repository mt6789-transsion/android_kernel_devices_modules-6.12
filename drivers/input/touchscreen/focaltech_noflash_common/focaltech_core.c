// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech Touchscreen Core Driver (SPI, NoFlash TDDI FT8725)
 */
#include "focaltech_core.h"

struct fts_ts_data *fts_data;

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
	int i = 0;
	u16 *cid = ts_data->ic_info.cid;

	if (cid[0] == 0)
		return -ENODATA;

	for (i = 0; i < FTS_MAX_COMPATIBLE_TYPE; i++) {
		if ((((cid[i] >> 8) & 0xFF) != 0) && (((cid[i] >> 8) & 0xFF) == id_h))
			return 0;
	}

	return -ENODATA;
}

int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 chip_idh = ts_data->ic_info.ids.chip_idh;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
			FTS_INFO("TP Ready,Device ID:0x%02x", idh);
			return 0;
		}
		FTS_DEBUG("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);
		cnt++;
		msleep(50);
	} while (cnt < FTS_WAIT_TP_READY_RETRY);

	return -EIO;
}

void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	FTS_DEBUG("tp reset");
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(1);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	if (hdelayms)
		msleep(hdelayms);

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);
	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}
	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);
	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}
	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

int fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = { 0xEB, 0xAA, 0x09 };

	if (fts_data->bus_type != BUS_TYPE_I2C)
		return 0;

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
		return ret;
	}

	msleep(10);
	buf[0] = buf[1] = buf[2] = 0;
	ret = fts_read(NULL, 0, buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd read fail");
	} else if ((buf[0] == 0xEB) && (buf[1] == 0xAA) && (buf[2] == 0x08)) {
		FTS_DEBUG("hidi2c change to stdi2c successful");
		ret = 0;
	} else {
		FTS_DEBUG("hidi2c change to stdi2c not support or fail");
		ret = -EIO;
	}

	return ret;
}

void fts_release_all_finger(void)
{
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;
	u32 finger_count = 0;
	u32 max_touches = ts_data->pdata->max_touch_number;

	mutex_lock(&ts_data->report_mutex);
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

	ts_data->touchs = 0;
	ts_data->key_state = 0;
	mutex_unlock(&ts_data->report_mutex);
}

int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct tran_tpd_ops *ops = NULL;

	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}

	if (get_tpd_ops(&fts_noflash_controller, &ops) && ops->get_gesture_status)
		ts_data->gesture_support = (ops->get_gesture_status() != 0);

	fts_esdcheck_suspend(ts_data);

	if (ts_data->gesture_support) {
		fts_gesture_suspend(ts_data);
	} else {
		fts_irq_disable();
		FTS_INFO("make TP enter into sleep mode");
		ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
	}

	fts_release_all_finger();
	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

int fts_ts_resume(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 cmd[2] = { 0 };

	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}

	ts_data->suspended = false;
	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell)
		fts_reset_proc(200);

	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);
	fts_esdcheck_resume(ts_data);

	if (fts_noflash_controller.ts) {
		if (fts_noflash_controller.ts->edge_reject_switch)
			fts_set_edge_reject_switch((u8)fts_noflash_controller.ts->edge_reject_switch);

		cmd[0] = FTS_REG_GAME_MODE_EN;
		cmd[1] = fts_noflash_controller.ts->game_state ? 1 : 0;
		ret = fts_write(cmd, 2);
		if (ret < 0)
			FTS_ERROR("write game mode fail");
	}

	if (ts_data->gesture_support)
		fts_gesture_resume(ts_data);
	else
		fts_irq_enable();

	FTS_FUNC_EXIT();
	return 0;
}

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data, resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int *x_dim = (int *)data->pdata->key_x_coords;
	int *y_dim = (int *)data->pdata->key_y_coords;

	if (!data->pdata->have_key)
		return -EINVAL;

	for (i = 0; i < data->pdata->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
		    (y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (FTS_EVENT_DOWN(data->events[index].flag) &&
			    !(data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 1);
				data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (FTS_EVENT_UP(data->events[index].flag) &&
				   (data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 0);
				data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}

	return -EINVAL;
}

static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = data->pdata->max_touch_number;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0)
			continue;

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);

		if (FTS_EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;

			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);
			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++) {
			if (BIT(i) & (data->touchs ^ touchs)) {
				va_reported = true;
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		if (FTS_EVENT_NO_DOWN(data) || !touchs)
			input_report_key(data->input_dev, BTN_TOUCH, 0);
		else
			input_report_key(data->input_dev, BTN_TOUCH, 1);
	}

	input_sync(data->input_dev);
	return 0;
}

static void fts_show_touch_buffer(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("touch_buf:%s", tmpbuf);
	kfree(tmpbuf);
}

static int fts_read_touchdata_spi(struct fts_ts_data *data, u8 *buf)
{
	int ret = 0;
	u8 cmd = 0x01;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x01;

	ret = fts_read(&cmd, 1, buf + 1, data->pnt_buf_size - 1);
	if ((buf[1] == 0xEF) && (buf[2] == 0xEF) && (buf[3] == 0xEF) && (buf[4] == 0xEF)) {
		fts_release_all_finger();
		fts_fw_recovery();
		data->fw_is_running = 1;
		return 1;
	}

	if (ret < 0) {
		FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
		return ret;
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data, u8 *touch_buf)
{
	int ret = 0;
	u8 gesture_en = 0xFF;

	memset(touch_buf, 0xFF, FTS_MAX_TOUCH_BUF);
	data->ta_size = data->touch_size;

	ret = fts_read_touchdata_spi(data, touch_buf);
	if (ret) {
		if (ret < 0)
			FTS_ERROR("read touchdata fails");
		return ret;
	}

	if (data->log_level >= 3)
		fts_show_touch_buffer(touch_buf, data->ta_size);

	if (data->suspended && data->gesture_support) {
		ret = fts_read_reg(FTS_REG_GESTURE_EN, &gesture_en);
		if ((ret >= 0) && (gesture_en == 1)) {
			fts_gesture_readdata(data, touch_buf);
			return 1;
		}
		FTS_DEBUG("gesture not enable in fw, don't process gesture");
	}

	if (data->ta_mode && data->ta_buf) {
		memcpy(data->ta_buf, touch_buf, data->ta_size);
		data->ta_flag = 1;
		wake_up_interruptible(&data->ts_waitqueue);
	}

	if ((touch_buf[1] == 0xFF) && (touch_buf[2] == 0xFF) &&
	    (touch_buf[3] == 0xFF) && (touch_buf[4] == 0xFF)) {
		FTS_INFO("touch buff is 0xff, need recovery state");
		fts_release_all_finger();
		fts_tp_state_recovery(data);
		return 1;
	}

	return 0;
}

static int fts_irq_read_report(struct fts_ts_data *ts_data)
{
	int i = 0;
	int max_touch_num = ts_data->pdata->max_touch_number;
	int touch_etype = 0;
	u8 event_num = 0;
	u8 finger_num = 0;
	u8 pointid = 0;
	u8 base = 0;
	u8 *touch_buf = ts_data->point_buf;
	struct ts_event *events = ts_data->events;
	int resolution = 1;

	if (fts_read_parse_touchdata(ts_data, touch_buf))
		return 0;

	touch_etype = (touch_buf[FTS_TOUCH_POINT_NUM] >> 4) & 0x0F;
	finger_num = touch_buf[FTS_TOUCH_POINT_NUM] & 0x0F;

	if (fts_noflash_controller.ts && fts_noflash_controller.ts->supplier)
		resolution = fts_noflash_controller.ts->supplier->resolution;

	if ((touch_etype == 0) && (finger_num <= max_touch_num)) {
		ts_data->point_num = finger_num;
		event_num = 0;

		for (i = 0; i < max_touch_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 2;
			pointid = (touch_buf[FTS_TOUCH_ID_POS + base - 2]) >> 4;
			if (pointid >= FTS_MAX_ID)
				break;
			if (pointid >= max_touch_num) {
				FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_EVENT_POS + base - 2] >> 6;

			if (resolution != 1) {
				/* 8x high-resolution subpixel coordinates */
				events[i].x = ((((touch_buf[FTS_TOUCH_X_H_POS + base - 2] & 0x0F) << 8) +
						touch_buf[FTS_TOUCH_X_L_POS + base - 2]) << 3) |
					      ((touch_buf[FTS_TOUCH_PRE_POS + base - 2] >> 4) & 0x07);
				events[i].y = ((((touch_buf[FTS_TOUCH_Y_H_POS + base - 2] & 0x0F) << 8) +
						touch_buf[FTS_TOUCH_Y_L_POS + base - 2]) << 3) |
					      (touch_buf[FTS_TOUCH_PRE_POS + base - 2] & 0x07);
				events[i].p = 0x3F;
				events[i].area = touch_buf[FTS_TOUCH_AREA_POS + base - 2];
				if (events[i].area == 0)
					events[i].area = 0x09;
				events[i].minor = events[i].area;
			} else {
				events[i].x = ((touch_buf[FTS_TOUCH_X_H_POS + base - 2] & 0x0F) << 8) +
					      touch_buf[FTS_TOUCH_X_L_POS + base - 2];
				events[i].y = ((touch_buf[FTS_TOUCH_Y_H_POS + base - 2] & 0x0F) << 8) +
					      touch_buf[FTS_TOUCH_Y_L_POS + base - 2];
				events[i].p = touch_buf[FTS_TOUCH_PRE_POS + base - 2];
				events[i].area = touch_buf[FTS_TOUCH_AREA_POS + base - 2];
				if (events[i].p == 0)
					events[i].p = 0x3F;
				if (events[i].area == 0)
					events[i].area = 0x09;
				events[i].minor = events[i].area;
			}

			event_num++;
			if (FTS_EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
				FTS_INFO("abnormal touch data from fw");
				return -EIO;
			}
		}

		if (event_num == 0) {
			FTS_INFO("no touch point information(%02x)", touch_buf[2]);
			return -EIO;
		}

		ts_data->touch_point = event_num;
		mutex_lock(&ts_data->report_mutex);
		fts_input_report_b(ts_data);
		mutex_unlock(&ts_data->report_mutex);
	}

	return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	struct fts_ts_data *ts_data = data;

	if (ts_data->suspended && ts_data->gesture_support)
		pm_wakeup_event(ts_data->dev, 2000);

	ts_data->intr_jiffies = jiffies;
	fts_prc_queue_work(ts_data);
	fts_irq_read_report(ts_data);

	return IRQ_HANDLED;
}

static int fts_get_dt_coords(struct device *dev, char *name,
			     struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[4] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != 4) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = 0;
		pdata->y_min = 0;
		pdata->x_max = 1080;
		pdata->y_max = 2460;
		return -ENODATA;
	}

	pdata->x_min = coords[0];
	pdata->y_min = coords[1];
	pdata->x_max = coords[2];
	pdata->y_max = coords[3];

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
		 pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;

	FTS_FUNC_ENTER();
	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys",
						 pdata->keys, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
						 pdata->key_x_coords, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
						 pdata->key_y_coords, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");
	}

	pdata->reset_gpio = of_get_named_gpio(np, "focaltech,reset-gpio", 0);
	if ((int)pdata->reset_gpio < 0)
		FTS_ERROR("Unable to get reset_gpio");

	pdata->irq_gpio = of_get_named_gpio(np, "focaltech,irq-gpio", 0);
	if ((int)pdata->irq_gpio < 0)
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			pdata->max_touch_number = 2;
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
		 pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_bus_init(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	ts_data->bus_tx_buf = kzalloc(FTS_BUS_BUF_LEN, GFP_KERNEL);
	if (!ts_data->bus_tx_buf) {
		FTS_ERROR("failed to allocate memory for bus_tx_buf");
		return -ENOMEM;
	}

	ts_data->bus_rx_buf = kzalloc(FTS_BUS_BUF_LEN, GFP_KERNEL);
	if (!ts_data->bus_rx_buf) {
		FTS_ERROR("failed to allocate memory for bus_rx_buf");
		kfree(ts_data->bus_tx_buf);
		ts_data->bus_tx_buf = NULL;
		return -ENOMEM;
	}
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	FTS_FUNC_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	input_dev->name = FTS_DRIVER_NAME;
	input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = ts_data->dev;
	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (pdata->have_key) {
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
	}

	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		return ret;
	}

	ts_data->input_dev = input_dev;
	fts_noflash_controller.input_dev = input_dev;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_buffer_init(struct fts_ts_data *ts_data)
{
	ts_data->touch_size = FTS_TOUCH_DATA_LEN;
	ts_data->pnt_buf_size = FTS_MAX_TOUCH_BUF;
	ts_data->point_buf = kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf");
		return -ENOMEM;
	}

	return 0;
}

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}
		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]reset gpio request failed");
			goto err_irq_gpio_dir;
		}
		ret = gpio_direction_output(data->pdata->reset_gpio, 1);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for reset gpio failed");
			goto err_reset_gpio_dir;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 cmd[4] = { 0 };
	u8 id[2] = { 0 };

	ts_data->ic_info.is_incell = true;
	ts_data->ic_info.hid_supported = false;
	ts_data->ic_info.ids.chip_idh = 0x87;
	ts_data->ic_info.ids.chip_idl = 0x25;
	ts_data->ic_info.ids.rom_idh = 0x87;
	ts_data->ic_info.ids.rom_idl = 0x25;
	ts_data->ic_info.ids.pb_idh = 0x87;
	ts_data->ic_info.ids.pb_idl = 0xA5;

	for (cnt = 0; cnt < 3; cnt++) {
		fts_reset_proc(0);
		mdelay(8);

		cmd[0] = 0x55;
		cmd[1] = 0xAA;
		ret = fts_write(cmd, 2);
		if (ret < 0) {
			FTS_ERROR("start cmd write fail");
			continue;
		}
		msleep(12);

		cmd[0] = 0x90;
		cmd[1] = 0x00;
		cmd[2] = 0x00;
		cmd[3] = 0x00;
		ret = fts_read(cmd, 4, id, 2);
		if (ret < 0) {
			FTS_DEBUG("read boot id fail,retry:%d", cnt);
			continue;
		}

		FTS_INFO("get ic information, chip id = 0x%02x%02x(cid type=0x%x)",
			 id[0], id[1], 0);
		if ((id[0] == 0x87) && ((id[1] == 0x25) || (id[1] == 0xA5)))
			return 0;

		FTS_DEBUG("can't get ic informaton,retry:%d", cnt);
	}

	FTS_ERROR("not focal IC, unregister driver");
	return -EIO;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	FTS_INFO("irq in ts_data:%d irq in client:%d", ts_data->irq, 0);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   FTS_DRIVER_NAME, ts_data);
	if (ret != 0)
		FTS_ERROR("request irq failed");

	return ret;
}

int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct fts_ts_platform_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);

	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
	if (ret)
		FTS_ERROR("device-tree parse fail");

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue)
		FTS_ERROR("create fts workqueue fail");

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);
	init_waitqueue_head(&ts_data->ts_waitqueue);
	INIT_WORK(&ts_data->resume_work, fts_resume_work);

	fts_data = ts_data;

	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("input initialize fail");
		goto err_input_init;
	}

	ret = fts_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("buffer init fail");
		goto err_buffer_init;
	}

	ret = fts_gpio_configure(ts_data);
	if (ret) {
		FTS_ERROR("configure the gpios fail");
		goto err_gpio_config;
	}

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_irq_req;
	}

	ret = fts_create_apk_debug_channel(ts_data);
	if (ret)
		FTS_ERROR("create apk debug node fail");

	ret = fts_create_sysfs(ts_data);
	if (ret)
		FTS_ERROR("create sysfs node fail");

	ret = fts_point_report_check_init(ts_data);
	if (ret)
		FTS_ERROR("init point report check fail");

	ret = fts_ex_mode_init(ts_data);
	if (ret)
		FTS_ERROR("init glove/cover/charger fail");

	fts_gesture_init(ts_data);

	ret = fts_test_init(ts_data);
	if (ret)
		FTS_ERROR("init production test fail");

	ret = fts_esdcheck_init(ts_data);
	if (ret)
		FTS_ERROR("init esd check fail");

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}

	ret = fts_fwupg_init(ts_data);
	if (ret)
		FTS_ERROR("init fw upgrade fail");

	device_init_wakeup(ts_data->dev, 1);

	FTS_FUNC_EXIT();
	return 0;

err_irq_req:
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_gpio_config:
	kfree(ts_data->point_buf);
err_buffer_init:
	input_unregister_device(ts_data->input_dev);
err_input_init:
	kfree(ts_data->bus_tx_buf);
	kfree(ts_data->bus_rx_buf);
err_bus_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
	kfree(ts_data->pdata);
	FTS_FUNC_EXIT();
	return ret;
}

int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	cancel_work_sync(&ts_data->resume_work);
	fts_point_report_check_exit(ts_data);
	fts_release_apk_debug_channel(ts_data);
	fts_remove_sysfs(ts_data);
	fts_ex_mode_exit(ts_data);
	fts_fwupg_exit(ts_data);
	fts_test_exit(ts_data);
	fts_esdcheck_exit(ts_data);
	fts_gesture_exit(ts_data);

	free_irq(ts_data->irq, ts_data);
	input_unregister_device(ts_data->input_dev);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

	kfree(ts_data->point_buf);
	kfree(ts_data->bus_tx_buf);
	kfree(ts_data->bus_rx_buf);
	kfree(ts_data->pdata);
	device_init_wakeup(ts_data->dev, 0);
	FTS_FUNC_EXIT();
	return 0;
}
