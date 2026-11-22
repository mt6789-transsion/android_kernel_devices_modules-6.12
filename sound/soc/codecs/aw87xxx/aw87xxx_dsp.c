/* SPDX-License-Identifier: GPL-2.0 */
#include "aw87xxx_dsp.h"
#include "aw87xxx_log.h"

bool aw_dsp_isEnable(void)
{
	return false;
}

int aw_dsp_get_rx_module_enable(int *enable)
{
	AW_LOGE("dsp is not enabled");
	return -EINVAL;
}

int aw_dsp_set_rx_module_enable(int enable)
{
	AW_LOGE("dsp is not enabled");
	return -EINVAL;
}

int aw_dsp_get_vmax(u32 *vmax, int dev_index)
{
	AW_LOGE("dsp is not enabled, dev_index=%d", dev_index);
	return -EINVAL;
}

int aw_dsp_set_vmax(u32 vmax, int dev_index)
{
	AW_LOGE("dsp is not enabled, dev_index=%d", dev_index);
	return -EINVAL;
}
