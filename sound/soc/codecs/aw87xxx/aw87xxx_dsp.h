/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AW87XXX_DSP_H__
#define __AW87XXX_DSP_H__

#include <linux/types.h>

bool aw_dsp_isEnable(void);
int aw_dsp_get_rx_module_enable(int *enable);
int aw_dsp_set_rx_module_enable(int enable);
int aw_dsp_get_vmax(u32 *vmax, int dev_index);
int aw_dsp_set_vmax(u32 vmax, int dev_index);

#endif /* __AW87XXX_DSP_H__ */
