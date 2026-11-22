/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _TRAN_COMMON_CLASS_H_
#define _TRAN_COMMON_CLASS_H_

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/types.h>

enum tran_common_prop {
	TRAN_PROP_UNKNOWN = 0,
	TRAN_PROP_WD_CMD = 1,
	TRAN_PROP_MAX,
};

union com_propval {
	int intval;
	const char *strval;
};

struct tran_properties {
	const char *alias_name;
};

struct tran_device;

struct tran_ops {
	int (*set_prop)(struct tran_device *tran_dev,
			enum tran_common_prop prop,
			const union com_propval *val);
	int (*get_prop)(struct tran_device *tran_dev,
			enum tran_common_prop prop,
			union com_propval *val);
};

/* Exact sizeof(struct tran_device) == 0x6c0 (1728 bytes) */
struct tran_device {
	struct tran_properties props;       /* +0x000 */
	const struct tran_ops *ops;         /* +0x008 */
	struct mutex ops_lock;              /* +0x010 */
	struct device dev;                  /* +0x040 */
	struct srcu_notifier_head evt_nh;   /* +0x3c0 */
	int log_level;                      /* +0x6b0 */
	bool is_polling_mode;               /* +0x6b4 */
	u8 _pad[11];                        /* +0x6b5..0x6bf */
};

#define to_tran_device(obj) container_of(obj, struct tran_device, dev)

static inline void *tran_dev_get_drvdata(const struct tran_device *tran_dev)
{
	return dev_get_drvdata(&tran_dev->dev);
}

static inline void tran_dev_set_drvdata(struct tran_device *tran_dev, void *data)
{
	dev_set_drvdata(&tran_dev->dev, data);
}

struct tran_device *tran_device_register(const char *name,
					 struct device *parent,
					 void *devdata,
					 const struct tran_ops *ops,
					 const struct tran_properties *props);
void tran_device_unregister(struct tran_device *tran_dev);
struct tran_device *tran_get_by_name(const char *name);
int tran_dev_set_prop(struct tran_device *tran_dev,
		      enum tran_common_prop prop,
		      const union com_propval *val);
int tran_dev_get_prop(struct tran_device *tran_dev,
		      enum tran_common_prop prop,
		      union com_propval *val);
int register_tran_device_notifier(struct tran_device *tran_dev,
				  struct notifier_block *nb);
int unregister_tran_device_notifier(struct tran_device *tran_dev,
				    struct notifier_block *nb);

#endif /* _TRAN_COMMON_CLASS_H_ */
