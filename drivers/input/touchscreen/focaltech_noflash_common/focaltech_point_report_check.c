// SPDX-License-Identifier: GPL-2.0
/*
 * FocalTech Point Report Check (stuck-touch release watchdog)
 */
#include "focaltech_core.h"

static void fts_prc_func(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
						   prc_work.work);
	unsigned long cur_jiffies = jiffies;
	unsigned long intr_jiffies = ts_data->intr_jiffies;
	unsigned int prc_ms = jiffies_to_msecs(cur_jiffies - intr_jiffies);

	if (prc_ms >= FTS_PRC_INTERVAL) {
		FTS_DEBUG("prc trigger interval:%dms", prc_ms);
		fts_release_all_finger();
	}
}

void fts_prc_queue_work(struct fts_ts_data *ts_data)
{
	if (ts_data->prc_support && ts_data->ts_workqueue) {
		cancel_delayed_work(&ts_data->prc_work);
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->prc_work,
				   msecs_to_jiffies(FTS_PRC_INTERVAL));
	}
}

static ssize_t fts_prc_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "prc:%s\n",
			 ts_data->prc_support ? "On" : "Off");
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static ssize_t fts_prc_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf))
		ts_data->prc_support = true;
	else if (FTS_SYSFS_ECHO_OFF(buf))
		ts_data->prc_support = false;
	mutex_unlock(&ts_data->input_dev->mutex);
	return count;
}

static DEVICE_ATTR(fts_prc, 0644, fts_prc_show, fts_prc_store);

int fts_point_report_check_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (ts_data->ts_workqueue) {
		INIT_DELAYED_WORK(&ts_data->prc_work, fts_prc_func);
	} else {
		FTS_ERROR("fts workqueue is NULL, can't run point report check function");
		return -EINVAL;
	}

	ts_data->prc_support = true;
	ret = sysfs_create_file(&ts_data->dev->kobj, &dev_attr_fts_prc.attr);
	if (ret)
		FTS_ERROR("create prc sysfs fail");

	FTS_FUNC_EXIT();
	return 0;
}

int fts_point_report_check_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	cancel_delayed_work_sync(&ts_data->prc_work);
	sysfs_remove_file(&ts_data->dev->kobj, &dev_attr_fts_prc.attr);
	FTS_FUNC_EXIT();
	return 0;
}
