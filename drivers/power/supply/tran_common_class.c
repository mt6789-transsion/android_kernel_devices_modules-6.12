// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 xtrbasement
 *
 * Transsion common power supply sysfs class driver.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "tran_common_class.h"

static struct class *tran_class;
struct srcu_notifier_head *tran_notifier_head;

static ssize_t tran_show_name(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct tran_device *tran_dev = to_tran_device(dev);

	return snprintf(buf, 20, "%s\n",
			tran_dev->props.alias_name ?
			tran_dev->props.alias_name : "anonymous");
}

static ssize_t show_tran_wd_cmd(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "null\n");
}

static ssize_t store_tran_wd_cmd(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct tran_device *tran_dev = to_tran_device(dev);
	int cmd = 0;
	int ret;
	union com_propval val = { 0 };

	ret = kstrtoint(buf, 0, &cmd);
	if (ret < 0)
		return ret;

	pr_err("%s: cmd:%d\n", __func__, cmd);
	val.intval = cmd;
	tran_dev_set_prop(tran_dev, TRAN_PROP_WD_CMD, &val);
	return size;
}

static DEVICE_ATTR(name, 0444, tran_show_name, NULL);
static DEVICE_ATTR(tran_wd_cmd, 0644, show_tran_wd_cmd, store_tran_wd_cmd);

static struct attribute *tran_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_tran_wd_cmd.attr,
	NULL,
};

static const struct attribute_group tran_group = {
	.attrs = tran_attrs,
};

static const struct attribute_group *tran_groups[] = {
	&tran_group,
	NULL,
};

static void tran_device_release(struct device *dev)
{
	struct tran_device *tran_dev = to_tran_device(dev);

	kfree(tran_dev);
}

int tran_dev_set_prop(struct tran_device *tran_dev,
		      enum tran_common_prop prop,
		      const union com_propval *val)
{
	if (tran_dev && tran_dev->ops && tran_dev->ops->set_prop)
		return tran_dev->ops->set_prop(tran_dev, prop, val);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(tran_dev_set_prop);

int tran_dev_get_prop(struct tran_device *tran_dev,
		      enum tran_common_prop prop,
		      union com_propval *val)
{
	if (tran_dev && tran_dev->ops && tran_dev->ops->get_prop)
		return tran_dev->ops->get_prop(tran_dev, prop, val);

	return -ENOTSUPP;
}
EXPORT_SYMBOL(tran_dev_get_prop);

int register_tran_device_notifier(struct tran_device *tran_dev,
				  struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&tran_dev->evt_nh, nb);
}
EXPORT_SYMBOL(register_tran_device_notifier);

int unregister_tran_device_notifier(struct tran_device *tran_dev,
				    struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&tran_dev->evt_nh, nb);
}
EXPORT_SYMBOL(unregister_tran_device_notifier);

struct tran_device *tran_device_register(const char *name,
					 struct device *parent,
					 void *devdata,
					 const struct tran_ops *ops,
					 const struct tran_properties *props)
{
	struct tran_device *tran_dev;
	char *dev_name;
	int rc;

	tran_dev = kzalloc(sizeof(*tran_dev), GFP_KERNEL);
	if (!tran_dev)
		return ERR_PTR(-ENOMEM);

	srcu_init_notifier_head(&tran_dev->evt_nh);
	mutex_init(&tran_dev->ops_lock);

	tran_dev->dev.class = tran_class;
	tran_dev->dev.parent = parent;
	tran_dev->dev.release = tran_device_release;

	dev_name = kasprintf(GFP_KERNEL, "%s", name);
	dev_set_name(&tran_dev->dev, dev_name);
	kfree(dev_name);

	dev_set_drvdata(&tran_dev->dev, devdata);
	if (props)
		tran_dev->props = *props;

	rc = device_register(&tran_dev->dev);
	if (rc) {
		kfree(tran_dev);
		return ERR_PTR(rc);
	}

	tran_dev->ops = ops;
	return tran_dev;
}
EXPORT_SYMBOL(tran_device_register);

void tran_device_unregister(struct tran_device *tran_dev)
{
	if (!tran_dev)
		return;

	mutex_lock(&tran_dev->ops_lock);
	tran_dev->ops = NULL;
	mutex_unlock(&tran_dev->ops_lock);
	device_unregister(&tran_dev->dev);
}
EXPORT_SYMBOL(tran_device_unregister);

static int tran_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct tran_device *tran_get_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return NULL;

	dev = class_find_device(tran_class, NULL, name, tran_match_device_by_name);
	return dev ? to_tran_device(dev) : NULL;
}
EXPORT_SYMBOL(tran_get_by_name);

static int __init tran_class_init(void)
{
	tran_class = class_create("tran_class");
	if (IS_ERR(tran_class)) {
		pr_warn("Unable to create transsion class; errno = %ld\n",
			PTR_ERR(tran_class));
		return PTR_ERR(tran_class);
	}

	tran_class->dev_groups = tran_groups;
	tran_notifier_head = kzalloc(sizeof(*tran_notifier_head), GFP_KERNEL);
	if (!tran_notifier_head)
		return -ENOMEM;

	srcu_init_notifier_head(tran_notifier_head);
	return 0;
}

static void __exit tran_class_exit(void)
{
	class_destroy(tran_class);
	kfree(tran_notifier_head);
}

subsys_initcall(tran_class_init);
module_exit(tran_class_exit);

MODULE_AUTHOR("shaoan zhu<shaoan.zhu@transsion.com>");
MODULE_DESCRIPTION("Switching Charger Class Device");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0_G");
