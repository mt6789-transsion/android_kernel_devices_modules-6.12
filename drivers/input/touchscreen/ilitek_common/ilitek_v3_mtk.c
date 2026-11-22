// SPDX-License-Identifier: GPL-2.0
/*
 * ILITEK TDDI MTK platform layer
 */
#include "ilitek_v3.h"

struct workqueue_struct *ini_wq;
struct delayed_work ini_work;

static const struct of_device_id mtk_tp_match_table[] = {
	{ .compatible = "tchip,ilitek" },
	{},
};

static int ilitek_plat_probe(void);
static int ilitek_plat_remove(void);

static struct ilitek_hwif_info hwif = {
	.bus_type = BUS_SPI,
	.plat_type = 0,
	.chip_id = ILI7807_CHIP,
	.name = "mtk-tpd",
	.owner = THIS_MODULE,
	.of_match_table = mtk_tp_match_table,
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
};

void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ilits->rst_edge_delay);
	if (!gpio_is_valid(ilits->tp_rst)) {
		ILI_ERR("tp_rst gpio is invalid\n");
		return;
	}

	gpio_direction_output(ilits->tp_rst, 1);
	mdelay(1);
	gpio_set_value(ilits->tp_rst, 0);
	mdelay(5);
	gpio_set_value(ilits->tp_rst, 1);
	mdelay(ilits->rst_edge_delay);
}

void ilitek_reset_output(int level)
{
	ILI_INFO("ilitek_reset_output level = %d\n", level);
	if (ilits && gpio_is_valid(ilits->tp_rst))
		gpio_direction_output(ilits->tp_rst, level);
}

void ili_input_register(void)
{
	int ret;
	int res = 1;

	ILI_INFO("Register input device\n");
	ilits->input = input_allocate_device();
	if (!ilits->input) {
		ILI_ERR("Failed to allocate input device\n");
		return;
	}

	ilits->input->name = "mtk-tpd";
	ilits->input->phys = "mtk-tpd/input0";
	ilits->input->id.bustype = BUS_SPI;
	ilits->input->dev.parent = ilits->dev;

	__set_bit(EV_SYN, ilits->input->evbit);
	__set_bit(EV_ABS, ilits->input->evbit);
	__set_bit(EV_KEY, ilits->input->evbit);
	__set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);
	__set_bit(BTN_TOUCH, ilits->input->keybit);

	/* Gesture & power keys (matching stock ili_input_register) */
	input_set_capability(ilits->input, EV_KEY, KEY_F13);
	input_set_capability(ilits->input, EV_KEY, KEY_POWER);
	input_set_capability(ilits->input, EV_KEY, KEY_UP);
	input_set_capability(ilits->input, EV_KEY, KEY_DOWN);
	input_set_capability(ilits->input, EV_KEY, KEY_LEFT);
	input_set_capability(ilits->input, EV_KEY, KEY_RIGHT);
	input_set_capability(ilits->input, EV_KEY, KEY_O);
	input_set_capability(ilits->input, EV_KEY, KEY_E);
	input_set_capability(ilits->input, EV_KEY, KEY_M);
	input_set_capability(ilits->input, EV_KEY, KEY_W);
	input_set_capability(ilits->input, EV_KEY, KEY_S);
	input_set_capability(ilits->input, EV_KEY, KEY_V);
	input_set_capability(ilits->input, EV_KEY, KEY_Z);
	input_set_capability(ilits->input, EV_KEY, KEY_C);
	input_set_capability(ilits->input, EV_KEY, KEY_F);

	if (ili7807_controller.ts && ili7807_controller.ts->supplier &&
	    ili7807_controller.ts->supplier->resolution > 0)
		res = ili7807_controller.ts->supplier->resolution;

	input_set_abs_params(ilits->input, ABS_MT_POSITION_X,
			     TOUCH_SCREEN_X_MIN,
			     (ilits->panel_wid ? ilits->panel_wid : TOUCH_SCREEN_X_MAX) * res - 1,
			     0, 0);
	input_set_abs_params(ilits->input, ABS_MT_POSITION_Y,
			     TOUCH_SCREEN_Y_MIN,
			     (ilits->panel_hei ? ilits->panel_hei : TOUCH_SCREEN_Y_MAX) * res - 1,
			     0, 0);
	input_set_abs_params(ilits->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);

	ret = input_register_device(ilits->input);
	if (ret < 0) {
		ILI_ERR("Failed to register input device, ret = %d\n", ret);
		input_free_device(ilits->input);
		ilits->input = NULL;
		return;
	}

	ili7807_controller.input_dev = ilits->input;
}

void ili_input_pen_register(void)
{
	int ret;

	ILI_INFO("Input Pen device for Single Touch\n");
	ilits->input_pen = input_allocate_device();
	if (!ilits->input_pen) {
		ILI_ERR("Failed to allocate pen input device\n");
		return;
	}

	ilits->input_pen->name = "ilitek_pen";
	ilits->input_pen->id.bustype = BUS_SPI;
	ilits->input_pen->dev.parent = ilits->dev;

	__set_bit(EV_SYN, ilits->input_pen->evbit);
	__set_bit(EV_ABS, ilits->input_pen->evbit);
	__set_bit(EV_KEY, ilits->input_pen->evbit);
	__set_bit(BTN_TOUCH, ilits->input_pen->keybit);
	__set_bit(BTN_TOOL_PEN, ilits->input_pen->keybit);
	__set_bit(BTN_STYLUS, ilits->input_pen->keybit);
	__set_bit(BTN_STYLUS2, ilits->input_pen->keybit);

	input_set_abs_params(ilits->input_pen, ABS_X, 0, TOUCH_SCREEN_X_MAX, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_Y, 0, TOUCH_SCREEN_Y_MAX, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_PRESSURE, 0, 4096, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_TILT_X, -90, 90, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_TILT_Y, -90, 90, 0, 0);

	ret = input_register_device(ilits->input_pen);
	if (ret < 0) {
		ILI_ERR("Failed to register pen input device, ret = %d\n", ret);
		input_free_device(ilits->input_pen);
		ilits->input_pen = NULL;
	}
}

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);
	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	disable_irq_nosync(ilits->irq_num);
	atomic_set(&ilits->irq_stat, DISABLE);
	ILI_DBG("Disable irq success\n");
out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);
	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	enable_irq(ilits->irq_num);
	atomic_set(&ilits->irq_stat, ENABLE);
	ILI_DBG("Enable irq success\n");
out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("CMD INT detected, ignore\n");
		wake_up(&ilits->inq);
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
		ilits->report,
		atomic_read(&ilits->tp_reset),
		atomic_read(&ilits->fw_stat),
		atomic_read(&ilits->tp_sw_mode),
		atomic_read(&ilits->mp_stat),
		atomic_read(&ilits->tp_sleep),
		atomic_read(&ilits->esd_stat));

	if (!ilits->report ||
	    atomic_read(&ilits->tp_reset) ||
	    atomic_read(&ilits->fw_stat) ||
	    atomic_read(&ilits->tp_sw_mode) ||
	    atomic_read(&ilits->mp_stat) ||
	    atomic_read(&ilits->tp_sleep) ||
	    atomic_read(&ilits->esd_stat)) {
		ILI_DBG("ignore interrupt !\n");
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	if (ilits->irq_num > 0)
		devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (!get_irq_pin) {
		ilits->irq_num = gpio_to_irq(ilits->tp_int);
		get_irq_pin = true;
	}

	ILI_INFO("ilits->irq_num = %d\n", ilits->irq_num);

	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
					ilitek_plat_isr_top_half,
					ilitek_plat_isr_bottom_half,
					type | IRQF_ONESHOT, "ilitek", NULL);
	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	else if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n",
			ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);
	return ret;
}

void ili_ts_suspend(struct device *dev)
{
	if (ili_sleep_handler(TP_DEEP_SLEEP) < 0)
		ILI_ERR("TP suspend failed\n");
}

void ili_ts_resume(struct device *dev)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

static void tran_request_ini_file(struct work_struct *work)
{
	int ret;

	ILI_INFO("Request ini file: %s\n", ilits->ini_name);
	if (!ilits || !ilits->dev || !ilits->ini_name[0])
		return;

	ret = request_firmware(&ilits->ini_fw, ilits->ini_name, ilits->dev);
	if (ret < 0)
		ILI_ERR("Failed to request ini file %s, ret = %d\n", ilits->ini_name, ret);
	else
		ILI_INFO("Request ini file %s success (size = %zu)\n",
			 ilits->ini_name, ilits->ini_fw->size);
}

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
	struct gpio_desc *int_desc, *rst_desc;

	int_desc = devm_gpiod_get(ilits->dev, "touch,irq", GPIOD_IN);
	if (IS_ERR(int_desc)) {
		ILI_ERR("Failed to get touch,irq-gpio: %ld\n", PTR_ERR(int_desc));
		return PTR_ERR(int_desc);
	}
	rst_desc = devm_gpiod_get(ilits->dev, "touch,reset", GPIOD_OUT_HIGH);
	if (IS_ERR(rst_desc)) {
		ILI_ERR("Failed to get touch,reset-gpio: %ld\n", PTR_ERR(rst_desc));
		return PTR_ERR(rst_desc);
	}

	ilits->tp_int = desc_to_gpio(int_desc);
	ilits->tp_rst = desc_to_gpio(rst_desc);

	ILI_INFO("TP INT: %d\n", ilits->tp_int);
	ILI_INFO("TP RESET: %d\n", ilits->tp_rst);
	return ret;
}

static int ilitek_plat_probe(void)
{
	int ret;

	ILI_INFO("platform probe\n");

	if (ilitek_plat_gpio_register() < 0) {
		ILI_ERR("Register gpio failed\n");
		return -EINVAL;
	}

	ili_irq_register(IRQF_TRIGGER_FALLING);

	ret = ili_tddi_init();
	if (ret < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		ili_irq_unregister();
		return ret;
	}

	ini_wq = alloc_workqueue("ini_wq", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (ini_wq) {
		INIT_DELAYED_WORK(&ini_work, tran_request_ini_file);
		queue_delayed_work(ini_wq, &ini_work, msecs_to_jiffies(3000));
	}

	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
	if (ini_wq) {
		cancel_delayed_work_sync(&ini_work);
		destroy_workqueue(ini_wq);
		ini_wq = NULL;
	}
	ili_dev_remove(true);
	return 0;
}

int ilitek_commmon_init(void)
{
	int ret;

	ILI_INFO("ilitek_commmon_init enter\n");
	ret = ili_dev_init(&hwif);
	if (ret < 0) {
		ILI_ERR("Failed to init ilitek device, ret = %d\n", ret);
		return ret;
	}
	return 0;
}
