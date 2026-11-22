// SPDX-License-Identifier: GPL-2.0
/*
 * Transsion touch panel MediaTek platform integration (X6837 / MT6789)
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "tran_touch_core.h"

struct tran_ps_data *g_ps_data;

static char final_ctp_firmware_version[16] = "0x1f";

/* MediaTek ATAG structs in /chosen */
struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[];
};

static ssize_t tp_final_fw_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	tran_info("tp_final_fw_version = %s\n", final_ctp_firmware_version);
	return sprintf(buf, "%s\n", final_ctp_firmware_version);
}

static ssize_t tp_final_fw_version_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	tran_info("tp_final_fw_version_store\n");
	strncpy(final_ctp_firmware_version, buf,
		sizeof(final_ctp_firmware_version) - 1);
	return count;
}

static ssize_t tp_vendor_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	char ic_upper[17] = { 0 };
	size_t len, i;

	if (!ctrl)
		return sprintf(buf, "UNKNOWN");

	len = strlen(ctrl->ic_name);
	if (len > 16)
		len = 16;
	memcpy(ic_upper, ctrl->ic_name, len);

	for (i = 0; i < len; i++) {
		if (ic_upper[i] >= 'a' && ic_upper[i] <= 'z')
			ic_upper[i] -= 32;
	}

	if (!g_tran_ts.supplier || !g_tran_ts.supplier->vendor)
		return sprintf(buf, "UNKNOWN");

	tran_info("vendor_name:%s_%s_%02d\n",
		  g_tran_ts.supplier->vendor, ic_upper, g_tran_ts.supplier->id);
	return sprintf(buf, "%s_%s_%02d",
		       g_tran_ts.supplier->vendor, ic_upper,
		       g_tran_ts.supplier->id);
}

static ssize_t tp_vendor_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	tran_func_in();
	return count;
}

static ssize_t tp_firmware_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct tran_ts_controller *ctrl = g_tran_ts.controller;
	u8 fw = 0, bv = 0, cfg = 0;

	if (ctrl && ctrl->ops && ctrl->ops->get_firmware_version &&
	    ctrl->ops->get_base_version) {
		fw = ctrl->ops->get_firmware_version();
		bv = ctrl->ops->get_base_version();
		if (ctrl->ops->get_cfg_version)
			cfg = ctrl->ops->get_cfg_version();
		tran_info("fw:0x%02x bv:0x%02x\n", fw, bv);
	}

	if (ctrl && ctrl->vendor && strcmp("goodix", ctrl->vendor) == 0)
		return snprintf(buf, 32, "0x%02x_0x%02x,bv:0x%02x_0x%02x\n",
				fw, cfg, fw, cfg);

	return snprintf(buf, 32, "0x%02x,bv:0x%02x\n", fw, bv);
}

static ssize_t tp_firmware_version_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	tran_info("tp_firmware_version_store\n");
	return count;
}

static DEVICE_ATTR(tp_firmware_version, 0644,
		   tp_firmware_version_show, tp_firmware_version_store);
static DEVICE_ATTR(tp_vendor, 0644,
		   tp_vendor_show, tp_vendor_store);
static DEVICE_ATTR(tp_final_firmware_version, 0664,
		   tp_final_fw_version_show, tp_final_fw_version_store);

static struct attribute *mtk_tp_info_attrs[] = {
	&dev_attr_tp_firmware_version.attr,
	&dev_attr_tp_vendor.attr,
	&dev_attr_tp_final_firmware_version.attr,
	NULL,
};

static const struct attribute_group g_mtk_paltform_group = {
	.attrs = mtk_tp_info_attrs,
};

int mtk_node_create(struct tran_ts_core *ts)
{
	int ret;

	tran_func_in();

	ret = sysfs_create_group(&ts->pdev->dev.kobj, &g_mtk_paltform_group);
	if (ret) {
		sysfs_remove_group(&ts->pdev->dev.kobj, &g_mtk_paltform_group);
		return ret;
	}

	ret = sysfs_create_link(&platform_bus.kobj, &ts->pdev->dev.kobj,
				"mtk-tpd");

	tran_func_out();
	return ret;
}

static int mtk_power_misc_psy_event(struct notifier_block *nb,
				    unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val = { 0 };

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);

	if (val.intval == POWER_SUPPLY_STATUS_CHARGING && !g_tran_ts.usb_in) {
		tran_info("notif charger mode in\n");
		g_tran_ts.usb_in = 1;
		schedule_work(&g_tran_ts.usb_work);
	} else if (val.intval == POWER_SUPPLY_STATUS_DISCHARGING &&
		   g_tran_ts.usb_in) {
		tran_info("notif charger mode out\n");
		g_tran_ts.usb_in = 0;
		schedule_work(&g_tran_ts.usb_work);
	}

	return NOTIFY_DONE;
}

int mtk_register_charger_notify(struct tran_ts_core *ts)
{
	g_tran_ts.psy_notifier.notifier_call = mtk_power_misc_psy_event;
	power_supply_reg_notifier(&ts->psy_notifier);
	return 0;
}

int mtk_unregister_charger_notify(void)
{
	return 0;
}

static int nvt_disp_notifier_callback(struct notifier_block *self,
				      unsigned long event, void *data)
{
	int *evdata = data;

	if (!evdata) {
		tran_err("wrong value v check notifier !!\n");
		return -1;
	}

	if (event == MTK_DISP_EVENT_BLANK) {
		if (*evdata == MTK_DISP_BLANK_UNBLANK ||
		    *evdata == MTK_DISP_BLANK_UNBLANK_AFTER) {
			tran_info("drm notify tp resume start\n");
			mutex_lock(&g_tran_ts.dsp_lock);
			queue_work(g_tran_ts.work_wq, &g_tran_ts.dsp_work);
		}
	} else if (event == MTK_DISP_EARLY_EVENT_BLANK) {
		if (*evdata == MTK_DISP_BLANK_POWERDOWN) {
			tran_info("drm notify tp suspends start\n");
			mutex_lock(&g_tran_ts.dsp_lock);
			cancel_work_sync(&g_tran_ts.dsp_work);

			if (!g_tran_ts.probe_done) {
				tran_info("vendor_probe_result is false!\n");
			} else {
				state_change_machine(TRAN_SUSPEND_TRIGGER,
						     g_tran_ts.state);
				g_tran_ts.controller->ops->suspend(NULL);
			}

			mutex_unlock(&g_tran_ts.dsp_lock);
			tran_info("drm notify tp suspends end\n");
		}
	}

	return 0;
}

static int mtk_report_proximity_event(unsigned int event)
{
	return 0;
}

static int mtk_get_proximity_state(unsigned int *state)
{
	return 0;
}

void proximity_init(struct tran_ts_core *ts)
{
	ts->tpd_ops->report_proximity_event = mtk_report_proximity_event;
	ts->tpd_ops->get_proximity_state = mtk_get_proximity_state;
	g_ps_data = &ts->ps_data;
	ts->ps_data.state = TRAN_PS_STATE_IDLE;
	ts->ps_data.event = TRAN_PS_EVENT_DEFAULT;
}

static int mtk_register_dsp_notify(struct tran_ts_core *ts)
{
	int ret;

	ts->disp_notifier.notifier_call = nvt_disp_notifier_callback;
	ret = mtk_disp_notifier_register("Tran Touch", &ts->disp_notifier);
	if (ret)
		tran_err("Failed to register disp notifier client:%d\n", ret);

	return ret;
}

static int mtk_unregister_dsp_notify(struct tran_ts_core *ts)
{
	int ret;

	ret = mtk_disp_notifier_unregister(&ts->disp_notifier);
	if (ret)
		tran_err("Error occurred while unregistering disp_notifier.\n\n");

	return ret;
}

static int mtk_plat_init(struct tran_ts_core *ts)
{
	tran_func_in();
	mtk_register_dsp_notify(ts);
	mtk_register_charger_notify(ts);
	mtk_node_create(ts);
	proximity_init(ts);
	tran_func_out();
	return 0;
}

static int mtk_plat_deinit(struct tran_ts_core *ts)
{
	tran_func_in();
	mtk_unregister_dsp_notify(ts);
	tran_func_out();
	return 0;
}

static int mtk_find_lcm_name(char *lcm_name)
{
	struct device_node *chosen;
	const struct tag_videolfb *tag;
	int size = 0;

	chosen = of_find_node_by_path("/chosen");
	if (!chosen) {
		tran_err("of_chosen not found\n");
		tran_err("lcm name is not found.just return unknow!!!\n");
		return 0;
	}

	tag = of_get_property(chosen, "atag,videolfb", &size);
	if (!tag) {
		tran_err("videolfb_tag not found\n");
		tran_err("lcm name is not found.just return unknow!!!\n");
		return 0;
	}

	if (strlen(tag->lcmname) < 64)
		memcpy(lcm_name, tag->lcmname, strlen(tag->lcmname));

	if (lcm_name) {
		tran_info("lcm_name=%s is find len:%d\n",
			  tag->lcmname, (int)strlen(tag->lcmname));
		return 1;
	}

	tran_err("videolfb_tag not found\n");
	tran_err("lcm name is not found.just return unknow!!!\n");
	return 0;
}

static int mtk_get_boot_mode(void)
{
	struct device_node *chosen;
	const struct tag_bootmode *tag;
	u32 mode;

	chosen = of_find_node_by_path("/chosen");
	if (!chosen) {
		tran_err("tran_get_boot_mode error \n\n");
		return TRAN_NORMAL_BOOT;
	}

	tag = of_get_property(chosen, "atag,boot", NULL);
	if (!tag) {
		tran_err("of_get_property bootmode error \n\n");
		return TRAN_NORMAL_BOOT;
	}

	mode = tag->bootmode;
	tran_info("bootmode is %x\n", mode);

	if (mode == 1 || mode == 4 || mode == 6)
		return TRAN_FACTORY_BOOT;

	/* Allow bootmode == 2 (RECOVERY_BOOT) for OrangeFox touchscreen! */
	if (mode == 8 || mode == 9) {
		tran_err("bootmode=%d don't load touch !\n", mode);
		return TRAN_NOT_RUN_MODE;
	}

	return TRAN_NORMAL_BOOT;
}

static struct tran_platform_ops mtk_plat_ops = {
	.plat_init = mtk_plat_init,
	.plat_deinit = mtk_plat_deinit,
	.find_lcm_name = mtk_find_lcm_name,
	.get_boot_mode = mtk_get_boot_mode,
};

static struct tran_platform_info mtk_plat = {
	.selftest_name = "nvt_selftest",
	.ops = &mtk_plat_ops,
	.private_data = NULL,
};

void init_tran_platform_info(void)
{
	g_tran_ts.platform = &mtk_plat;
}
