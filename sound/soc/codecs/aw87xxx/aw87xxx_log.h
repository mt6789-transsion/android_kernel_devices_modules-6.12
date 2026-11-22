/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_LOG_H__
#define __AW87XXX_LOG_H__

#include <linux/device.h>
#include <linux/printk.h>

#define AW_DEV_LOGE(dev, format, ...) \
	pr_err("[Awinic] [%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_DEV_LOGI(dev, format, ...) \
	pr_info("[Awinic] [%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_DEV_LOGD(dev, format, ...) \
	pr_debug("[Awinic] [%s]%s: " format "\n", dev_name(dev), __func__, ##__VA_ARGS__)

#define AW_LOGE(format, ...) \
	pr_err("[Awinic] %s:" format "\n", __func__, ##__VA_ARGS__)

#define AW_LOGI(format, ...) \
	pr_info("[Awinic] %s:" format "\n", __func__, ##__VA_ARGS__)

#define AW_LOGD(format, ...) \
	pr_debug("[Awinic] %s:" format "\n", __func__, ##__VA_ARGS__)

#endif /* __AW87XXX_LOG_H__ */
