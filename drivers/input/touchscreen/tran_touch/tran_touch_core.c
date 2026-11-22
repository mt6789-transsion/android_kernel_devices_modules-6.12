// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion touch panel core framework (X6837 / MT6789)
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "tran_touch_core.h"

struct ts_state_entry {
	u32 trigger;
	u32 func_mode;
	u32 cur_state;
	u32 next_state;
};

static const struct ts_state_entry ts_state_machine[] = {
	{ TRAN_RESUME_TRIGGER,  0, TRAN_SUSPEND_MODE,         TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  0, TRAN_PS_SUSPEND_MODE,      TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  0, TRAN_GESTURE_SUSPEND_MODE, TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  2, TRAN_GESTURE_SUSPEND_MODE, TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  2, TRAN_PS_SUSPEND_MODE,      TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  2, TRAN_SUSPEND_MODE,         TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  1, TRAN_PS_SUSPEND_MODE,      TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  1, TRAN_GESTURE_SUSPEND_MODE, TRAN_ACTIVE_MODE },
	{ TRAN_RESUME_TRIGGER,  1, TRAN_SUSPEND_MODE,         TRAN_ACTIVE_MODE },
	{ TRAN_SUSPEND_TRIGGER, 0, TRAN_ACTIVE_MODE,          TRAN_SUSPEND_MODE },
	{ TRAN_SUSPEND_TRIGGER, 2, TRAN_ACTIVE_MODE,          TRAN_GESTURE_SUSPEND_MODE },
	{ TRAN_SUSPEND_TRIGGER, 1, TRAN_ACTIVE_MODE,          TRAN_PS_SUSPEND_MODE },
};

static void controller_init_finish(void)
{
	g_tran_ts.probe_done = 1;
	complete(&g_tran_ts.init_done);
	tran_info("controller load finish\n");
}

struct tran_tpd_ops g_tpd_ops = {
	.controller_init_finish = controller_init_finish,
};

struct tran_ts_core g_tran_ts = {
	.dsp_lock = __MUTEX_INITIALIZER(g_tran_ts.dsp_lock),
	.list_lock = __MUTEX_INITIALIZER(g_tran_ts.list_lock),
	.supplier_list = LIST_HEAD_INIT(g_tran_ts.supplier_list),
	.controller_list = LIST_HEAD_INIT(g_tran_ts.controller_list),
	.tpd_ops = &g_tpd_ops,
};

static const char *state_to_str(int state)
{
	switch (state) {
	case TRAN_ACTIVE_MODE:
		return "TRAN_ACTIVE_MODE";
	case TRAN_SUSPEND_MODE:
		return "TRAN_SUSPEND_MODE";
	case TRAN_PS_SUSPEND_MODE:
		return "TRAN_PS_SUSPEND_MODE";
	case TRAN_GESTURE_SUSPEND_MODE:
		return "TRAN_GESTURE_SUSPEND_MODE";
	default:
		return "unknow";
	}
}

int state_change_machine(int trigger, int cur_state)
{
	int ps_state, ges_state, func_mode;
	int i;

	if (trigger == TRAN_RESUME_TRIGGER && cur_state == TRAN_ACTIVE_MODE) {
		printk(KERN_INFO "[tran_touch]%s %d:%s can't change state, current %s\n",
		       __func__, __LINE__, "RESUME_TRIGER", state_to_str(cur_state));
		return 0;
	}

	if (trigger == TRAN_SUSPEND_TRIGGER &&
	    (cur_state == TRAN_SUSPEND_MODE ||
	     cur_state == TRAN_PS_SUSPEND_MODE ||
	     cur_state == TRAN_GESTURE_SUSPEND_MODE)) {
		printk(KERN_INFO "[tran_touch]%s %d:%s can't change state, current %s\n",
		       __func__, __LINE__, "SUSPEND_TRIGER", state_to_str(cur_state));
		return 0;
	}

	ps_state = g_tran_ts.tpd_ops->get_proximity_state(NULL);
	ges_state = g_tran_ts.tpd_ops->get_gesture_status();

	if (ps_state == TRAN_PS_STATE_NEAR)
		func_mode = 1;
	else if (ges_state != 0)
		func_mode = 2;
	else
		func_mode = 0;

	printk(KERN_INFO "[tran_touch]%s %d:func mode: %d, current state: %d \n",
	       __func__, __LINE__, func_mode, cur_state);

	for (i = 0; i < ARRAY_SIZE(ts_state_machine); i++) {
		if (ts_state_machine[i].trigger == (u32)trigger &&
		    ts_state_machine[i].func_mode == (u32)func_mode &&
		    ts_state_machine[i].cur_state == (u32)cur_state) {
			g_tran_ts.state = ts_state_machine[i].next_state;
			tran_info("core mode change %s --> %s\n",
				  state_to_str(cur_state),
				  state_to_str(g_tran_ts.state));
			break;
		}
	}

	return 0;
}

int register_ts_controller(struct tran_ts_controller *controller)
{
	struct tran_controller_node *node;
	int res;

	if (!controller || g_tran_ts.boot_mode == TRAN_NOT_RUN_MODE) {
		tran_info("Don't register reason %s\n",
			  controller ? "TRAN_NOT_RUN_MODE" : "controller is null");
		return -1;
	}

	node = vmalloc(sizeof(*node));
	if (!node) {
		tran_err("request controllerList mem failed\n");
		return -1;
	}

	node->controller = controller;
	node->list.next = NULL;
	node->list.prev = NULL;

	mutex_lock(&g_tran_ts.list_lock);
	list_add_tail(&node->list, &g_tran_ts.controller_list);
	mutex_unlock(&g_tran_ts.list_lock);

	res = queue_work(g_tran_ts.init_wq, &g_tran_ts.match_work);
	if (!res)
		tran_err("touch device queue_work failed\n\n");

	return 0;
}
EXPORT_SYMBOL_GPL(register_ts_controller);

int unregister_ts_controller(struct tran_ts_controller *controller)
{
	return 0;
}
EXPORT_SYMBOL_GPL(unregister_ts_controller);

static struct tran_ts_controller *check_support_ic(struct tran_ts_controller *ctrl,
						   const char *supplier_name)
{
	int i = 0;

	if (!ctrl || !ctrl->support_list)
		return NULL;

	while (ctrl->support_list[i]) {
		tran_info("Check whether the supplier supports %s\n",
			  ctrl->support_list[i]);
		if (strstr(supplier_name, ctrl->support_list[i])) {
			ctrl->ic_name = ctrl->support_list[i];
			ctrl->ts = &g_tran_ts;
			return ctrl;
		}
		i++;
	}

	return NULL;
}

static struct tran_ts_controller *match_tp_controller(void)
{
	char lcm_name[64];
	struct tran_supplier_info *sup;
	struct tran_controller_node *node;

	memset(lcm_name, 0, sizeof(lcm_name));

	if (g_tran_ts.platform && g_tran_ts.platform->ops &&
	    g_tran_ts.platform->ops->find_lcm_name) {
		g_tran_ts.platform->ops->find_lcm_name(lcm_name);
		tran_info("get_lcm_name %s\n", lcm_name);
	}

	list_for_each_entry(sup, &g_tran_ts.supplier_list, list) {
		tran_info("check supplier-%d %s\n", sup->id, sup->name);
		if (strstr(sup->name, lcm_name)) {
			tran_info("apply supplier-%d %s\n", sup->id, sup->name);
			g_tran_ts.supplier = sup;
			break;
		}
		if (strstr(sup->name, "any")) {
			tran_info("apply supplier-%d %s\n", sup->id, sup->name);
			g_tran_ts.supplier = sup;
			break;
		}
		g_tran_ts.supplier = NULL;
	}

	if (!g_tran_ts.supplier) {
		tran_err("no supplier match!!\n");
		return NULL;
	}

	mutex_lock(&g_tran_ts.list_lock);
	list_for_each_entry(node, &g_tran_ts.controller_list, list) {
		struct tran_ts_controller *ctrl = node->controller;

		if (!ctrl || !ctrl->vendor) {
			mutex_unlock(&g_tran_ts.list_lock);
			return NULL;
		}

		tran_info("try to find %s controller\n", ctrl->vendor);
		if (strstr(g_tran_ts.supplier->name, ctrl->vendor)) {
			g_tran_ts.controller =
				check_support_ic(ctrl, g_tran_ts.supplier->name);
		}
	}
	mutex_unlock(&g_tran_ts.list_lock);

	if (!g_tran_ts.controller) {
		tran_err("no controller match exit 0\n");
		return NULL;
	}

	tran_info("start load controller vendor:%s ic:%s\n",
		  g_tran_ts.controller->vendor,
		  g_tran_ts.controller->ic_name);

	return g_tran_ts.controller;
}

static void controller_match_work(struct work_struct *work)
{
	struct tran_ts_controller *ctrl;
	unsigned long ret;

	if (g_tran_ts.controller) {
		tran_info("Not allow to match controller\n");
		return;
	}

	ctrl = match_tp_controller();
	if (!ctrl)
		return;

	ctrl->ops->init();

	ret = wait_for_completion_timeout(&g_tran_ts.init_done, 2000);
	if (!ret) {
		tran_err("init controller timeout!\n");
		g_tran_ts.controller = NULL;
		return;
	}

	tran_info("load controller success!\n");

	if (g_tran_ts.platform && g_tran_ts.platform->ops &&
	    g_tran_ts.platform->ops->plat_init)
		g_tran_ts.platform->ops->plat_init(&g_tran_ts);

	gesture_init(&g_tran_ts);
	tran_node_init(&g_tran_ts);
}

static void dsp_event_callback(struct work_struct *work)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;

	if (!g_tran_ts.probe_done) {
		tran_info("vendor_probe_result is false!\n");
	} else {
		ctrl->ops->resume(NULL);
	}

	state_change_machine(TRAN_RESUME_TRIGGER, g_tran_ts.state);

	if (g_tran_ts.tpd_ops->get_proximity_state(NULL) == TRAN_PS_STATE_FAR)
		ctrl->ops->set_ts_mode(TRAN_MODE_PROXIMITY, 1);

	mutex_unlock(&g_tran_ts.dsp_lock);
	tran_info("drm notify tp resume end\n");
}

static void usb_event_callback(struct work_struct *work)
{
	u8 usb_in;

	if (g_tran_ts.state == TRAN_SUSPEND_MODE) {
		tran_info("in suspend cancel ops\n");
		return;
	}

	if (!g_tran_ts.probe_done) {
		tran_info("controller probe is not finish!\n");
		return;
	}

	usb_in = g_tran_ts.usb_in;
	g_tran_ts.controller->ops->set_ts_mode(TRAN_MODE_CHARGER, g_tran_ts.usb_in);

	if (usb_in)
		tran_info("noise high mode in state = %d\n", g_tran_ts.usb_in);
	else
		tran_info("noise normal mode in state = %d\n", g_tran_ts.usb_in);
}

static void tran_parse_dt(struct device_node *np)
{
	u32 supplier_num = 0;
	u32 resolution = 1;
	u32 func_val = 0;
	char prop_name[32];
	int prop_len = 0;
	int i, ret;

	tran_func_in();

	ret = of_property_read_u32(np, "supplier-num", &supplier_num);
	if (ret < 0) {
		tran_err("supplier-num: can not find!\n");
		return;
	}

	tran_info("supplier-num: %d\n", supplier_num);

	for (i = 0; i < (int)supplier_num; i++) {
		const char *sup_name;
		const char *sup_vendor;
		struct tran_supplier_info *info;

		memset(prop_name, 0, sizeof(prop_name));
		snprintf(prop_name, 11, "supplier-%d", i);
		sup_name = of_get_property(np, prop_name, &prop_len);
		if (!sup_name) {
			tran_err("supplier-%d: can not find!\n", i);
			return;
		}
		tran_info("supplier-%d: %s\n", i, sup_name);

		snprintf(prop_name, 18, "supplier-vendor-%d", i);
		sup_vendor = of_get_property(np, prop_name, &prop_len);
		if (!sup_vendor)
			tran_info("supplier-vendor-%d: can not find!\n", i);
		else
			tran_info("supplier-vendor-%d: %s\n", i, sup_vendor);

		snprintf(prop_name, 22, "supplier-resolution-%d", i);
		ret = of_property_read_u32(np, prop_name, &resolution);
		if (ret < 0) {
			tran_err("supplier-resolution-%d: can not find!\n", i);
			resolution = 1;
		}
		tran_info("supplier-resolution-%d: %d\n", i, resolution);

		info = vmalloc(sizeof(*info));
		if (!info) {
			tran_err("request supplier_list mem failed!\n");
			return;
		}

		info->id = i;
		info->resolution = (int)resolution;
		info->name = sup_name;
		info->vendor = sup_vendor;
		list_add_tail(&info->list, &g_tran_ts.supplier_list);
	}

	ret = of_property_read_u32(np, "psensor_function", &func_val);
	if (ret < 0)
		g_tran_ts.func_enable &= ~TRAN_FUNC_PSENSOR_BIT;
	else
		g_tran_ts.func_enable |= func_val;
	tran_info("pesnsor_function: %s\n",
		  (g_tran_ts.func_enable & TRAN_FUNC_PSENSOR_BIT) ? "on" : "off");

	ret = of_property_read_u32(np, "gesture_function", &func_val);
	if (ret < 0)
		g_tran_ts.func_enable |= TRAN_FUNC_GESTURE_BIT;
	else
		g_tran_ts.func_enable |= (func_val << 1);
	tran_info("gesture_function: %s\n",
		  (g_tran_ts.func_enable & TRAN_FUNC_GESTURE_BIT) ? "on" : "off");

	tran_func_out();
}

static int tpd_probe(struct platform_device *pdev)
{
	tran_func_in();

	if (!g_tran_ts.platform) {
		tran_err("tran platform is NULL!\n");
		return 0;
	}

	INIT_WORK(&g_tran_ts.match_work, controller_match_work);
	INIT_WORK(&g_tran_ts.dsp_work, dsp_event_callback);
	INIT_WORK(&g_tran_ts.usb_work, usb_event_callback);
	init_completion(&g_tran_ts.init_done);

	tran_parse_dt(pdev->dev.of_node);

	g_tran_ts.pdev = pdev;
	g_tran_ts.state = TRAN_ACTIVE_MODE;
	g_tran_ts.work_wq = create_singlethread_workqueue("tpd_work_queue");

	tran_func_out();
	return 0;
}

static void tpd_remove(struct platform_device *pdev)
{
	if (g_tran_ts.platform && g_tran_ts.platform->ops &&
	    g_tran_ts.platform->ops->plat_deinit)
		g_tran_ts.platform->ops->plat_deinit(&g_tran_ts);
}

static void tran_shutdown(struct platform_device *pdev)
{
	tran_func_in();

	if (g_tran_ts.platform && g_tran_ts.platform->ops &&
	    g_tran_ts.platform->ops->plat_deinit)
		g_tran_ts.platform->ops->plat_deinit(&g_tran_ts);
}

const struct of_device_id touch_of_match[] = {
	{ .compatible = "mediatek,touch" },
	{ .compatible = "transsion,touch" },
	{},
};

const struct dev_pm_ops tpd_pm_ops = {};

static struct platform_driver tpd_driver = {
	.probe = tpd_probe,
	.remove = tpd_remove,
	.shutdown = tran_shutdown,
	.driver = {
		.name = "tran-tpd",
		.owner = THIS_MODULE,
		.of_match_table = touch_of_match,
		.pm = &tpd_pm_ops,
	},
};

static void tpd_init_work_callback(struct work_struct *work)
{
	tran_func_in();

	if (platform_driver_register(&tpd_driver))
		tran_err("unable to register touch panel driver.\n\n");

	tran_func_out();
}

static void tran_abi_layout_check(void)
{
	BUILD_BUG_ON(sizeof(struct tran_ts_controller) != 0x48);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, ic_name) != 0x00);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, vendor) != 0x08);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, gesture_maps) != 0x10);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, selftest_data) != 0x18);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, support_list) != 0x20);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, input_dev) != 0x28);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, ops) != 0x30);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, ext_ops) != 0x38);
	BUILD_BUG_ON(offsetof(struct tran_ts_controller, ts) != 0x40);

	BUILD_BUG_ON(sizeof(struct tran_ts_controller_ops) != 0x58);
	BUILD_BUG_ON(sizeof(struct tran_ts_controller_ext_ops) != 0x10);
	BUILD_BUG_ON(sizeof(struct tran_tpd_ops) != 0x30);
	BUILD_BUG_ON(sizeof(struct tran_supplier_info) != 0x28);
	BUILD_BUG_ON(sizeof(struct tran_controller_node) != 0x18);

	/* 5.10 ABI size/offset checks disabled on 6.12 */
#if 0
	BUILD_BUG_ON(offsetof(struct tran_ts_core, edge_reject_switch) != 0x00);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, boot_mode) != 0x04);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, usb_in) != 0x08);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, probe_done) != 0x09);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, state) != 0x0c);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, ps_data) != 0x10);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, fod_down) != 0x1c);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, game_state) != 0x20);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, transient_report_rate) != 0x21);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, report_rate) != 0x22);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, game_mode) != 0x23);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, func_enable) != 0x24);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, pdev) != 0x28);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, dsp_lock) != 0x30);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, list_lock) != 0x60);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, init_done) != 0x90);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, init_wq) != 0xb0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, work_wq) != 0xb8);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, init_work) != 0xc0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, dsp_work) != 0xf0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, usb_work) != 0x120);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, match_work) != 0x150);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, disp_notifier) != 0x180);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, psy_notifier) != 0x198);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, supplier_list) != 0x1b0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, controller_list) != 0x1c0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, tpd_ops) != 0x1d0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, supplier) != 0x1d8);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, controller) != 0x1e0);
	BUILD_BUG_ON(offsetof(struct tran_ts_core, platform) != 0x1e8);
#endif
}

static int __init tpd_device_init(void)
{
	int res;

	tran_abi_layout_check();
	init_tran_platform_info();

	if (g_tran_ts.platform && g_tran_ts.platform->ops &&
	    g_tran_ts.platform->ops->get_boot_mode) {
		int mode = g_tran_ts.platform->ops->get_boot_mode();

		g_tran_ts.boot_mode = mode;
		if (mode != TRAN_NORMAL_BOOT && mode != TRAN_FACTORY_BOOT) {
			tran_err("bootmode don't load touch !\n");
			return 0;
		}
		tran_info("bootmode %d load touch !\n", mode);
	}

	g_tran_ts.init_wq = create_singlethread_workqueue("tpd_init_queue");
	INIT_WORK(&g_tran_ts.init_work, tpd_init_work_callback);

	res = queue_work(g_tran_ts.init_wq, &g_tran_ts.init_work);
	if (!res)
		tran_err("touch device queue_work failed res:%d\n\n", res);

	return 0;
}

static void __exit tpd_device_exit(void)
{
	tran_func_in();
	platform_driver_unregister(&tpd_driver);
	tran_func_out();
}

module_init(tpd_device_init);
module_exit(tpd_device_exit);

MODULE_AUTHOR("<Transsion_BSP@transsion.com>");
MODULE_DESCRIPTION("Transsion touch panel driver");
MODULE_LICENSE("GPL");
