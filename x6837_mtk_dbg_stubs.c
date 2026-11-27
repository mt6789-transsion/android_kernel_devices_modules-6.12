// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal stubs for optional MediaTek debug/profiling/QoS/MML hooks
 * so core MT6789 6.12 display/IOMMU/CMDQ modules link cleanly without
 * pulling in 30+ non-MT6789 (MML/DPC/telemetry) modules during bringup.
 */
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/thermal.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <drm/drm_gem.h>

const struct vm_operations_struct drm_gem_dma_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};
EXPORT_SYMBOL_GPL(drm_gem_dma_vm_ops);

s32 mtk_smi_dbg_hang_detect(char *user) { return 0; }
EXPORT_SYMBOL_GPL(mtk_smi_dbg_hang_detect);

void mtk_smi_dbg_dump_for_disp(void) {}
EXPORT_SYMBOL_GPL(mtk_smi_dbg_dump_for_disp);

int mtk_smi_dbg_register_notifier(struct notifier_block *nb) { return 0; }
EXPORT_SYMBOL_GPL(mtk_smi_dbg_register_notifier);

int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb) { return 0; }
EXPORT_SYMBOL_GPL(mtk_smi_dbg_unregister_notifier);

void mt_irq_dump_status(unsigned int irq) {}
EXPORT_SYMBOL_GPL(mt_irq_dump_status);

int mtk_iommu_register_fault_callback(int port, void *fn, void *cb_data, bool is_vpu) { return 0; }
EXPORT_SYMBOL_GPL(mtk_iommu_register_fault_callback);

void mmdvfs_debug_status_dump(void *seq) {}
EXPORT_SYMBOL_GPL(mmdvfs_debug_status_dump);

void mmdvfs_stop_record(void) {}
EXPORT_SYMBOL_GPL(mmdvfs_stop_record);

void mmqos_hrt_dump(void) {}
EXPORT_SYMBOL_GPL(mmqos_hrt_dump);

void mmqos_stop_record(void) {}
EXPORT_SYMBOL_GPL(mmqos_stop_record);

int mtk_dramc_get_ddr_type(void) { return 5; /* LPDDR4X */ }
EXPORT_SYMBOL_GPL(mtk_dramc_get_ddr_type);

void mtk_emiisu_record_off(void) {}
EXPORT_SYMBOL_GPL(mtk_emiisu_record_off);

void mtk_set_mmmc_rg(u32 a, u32 b) {}
EXPORT_SYMBOL_GPL(mtk_set_mmmc_rg);

/* MML-DRM hooks are now provided by real mtk-mml.ko */

/* MT6789 has no DisplayPort hardware; stub unused mtk_dp.o symbols */
ssize_t drm_dp_dpcd_read(void *aux, unsigned int offset, void *buffer, size_t size) { return -ENODEV; }
EXPORT_SYMBOL_GPL(drm_dp_dpcd_read);
ssize_t drm_dp_dpcd_write(void *aux, unsigned int offset, void *buffer, size_t size) { return -ENODEV; }
EXPORT_SYMBOL_GPL(drm_dp_dpcd_write);
void drm_dp_link_train_clock_recovery_delay(void *aux, const u8 *dpcd) {}
EXPORT_SYMBOL_GPL(drm_dp_link_train_clock_recovery_delay);
bool drm_dp_clock_recovery_ok(const u8 *link_status, int lane_count) { return false; }
EXPORT_SYMBOL_GPL(drm_dp_clock_recovery_ok);
void drm_dp_link_train_channel_eq_delay(void *aux, const u8 *dpcd) {}
EXPORT_SYMBOL_GPL(drm_dp_link_train_channel_eq_delay);
bool drm_dp_channel_eq_ok(const u8 *link_status, int lane_count) { return false; }
EXPORT_SYMBOL_GPL(drm_dp_channel_eq_ok);
void drm_dp_aux_init(void *aux) {}
EXPORT_SYMBOL_GPL(drm_dp_aux_init);
int drm_dp_aux_register(void *aux) { return -ENODEV; }
EXPORT_SYMBOL_GPL(drm_dp_aux_register);

/* MT6789 has gpueb_mode=off; stub optional GED/GPUEB/QoS hooks */
int gpufreq_get_cur_temperature(void) { return 35000; }
EXPORT_SYMBOL_GPL(gpufreq_get_cur_temperature);
void MTKGPUQoS_mode_ratio(u32 a, u32 b) {}
EXPORT_SYMBOL_GPL(MTKGPUQoS_mode_ratio);
/* set_gpu_pre_throttle / set_gpu_pre_throttle_opp are provided by thermal_interface.ko */
void (*ged_notify_gpu_fix_opp_fp)(int opp);
EXPORT_SYMBOL_GPL(ged_notify_gpu_fix_opp_fp);
void (*ged_notify_gpu_fix_freq_volt_fp)(unsigned int freq, unsigned int volt);
EXPORT_SYMBOL_GPL(ged_notify_gpu_fix_freq_volt_fp);
int mtk_ipi_send_compl_to_gpueb(int a, int b, void *c, int d, unsigned long e) { return -ENODEV; }
EXPORT_SYMBOL_GPL(mtk_ipi_send_compl_to_gpueb);
void *get_gpueb_ipidev(void) { return NULL; }
EXPORT_SYMBOL_GPL(get_gpueb_ipidev);
int gpueb_get_send_PIN_ID_by_name(char *name) { return -1; }
EXPORT_SYMBOL_GPL(gpueb_get_send_PIN_ID_by_name);
int gpueb_get_recv_PIN_ID_by_name(char *name) { return -1; }
EXPORT_SYMBOL_GPL(gpueb_get_recv_PIN_ID_by_name);

/* SCP optional debug/semaphore hooks */
int aee_get_mode(void) { return 0; }
EXPORT_SYMBOL_GPL(aee_get_mode);
void gpio_dump_regs_range(int start, int end) {}
EXPORT_SYMBOL_GPL(gpio_dump_regs_range);

/* Thermal / AUXADC optional hooks (get_apu_temp, set_cold_interrupt_enable_addr, set_reboot_temperature are in thermal_interface.ko) */
u32 get_devinfo_with_index(u32 index) { return 0; }
EXPORT_SYMBOL_GPL(get_devinfo_with_index);

/* Provide mutt-pa1 cooling device + ap_ntc fallback zone for Android ThermalHAL */
static unsigned long mutt_cur_state;
static int mutt_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 10;
	return 0;
}
static int mutt_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = mutt_cur_state;
	return 0;
}
static int mutt_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	mutt_cur_state = state;
	return 0;
}
static const struct thermal_cooling_device_ops mutt_cdev_ops = {
	.get_max_state = mutt_get_max_state,
	.get_cur_state = mutt_get_cur_state,
	.set_cur_state = mutt_set_cur_state,
};

static struct thermal_cooling_device *x6837_mutt_cdev;
static struct thermal_cooling_device *x6837_devfreq_cdev;
static struct thermal_cooling_device *x6837_flash_cdev;

static int __init x6837_dbg_stubs_init(void)
{
	x6837_mutt_cdev = thermal_cooling_device_register("mutt-pa1", NULL, &mutt_cdev_ops);
	if (IS_ERR(x6837_mutt_cdev))
		x6837_mutt_cdev = NULL;

	x6837_devfreq_cdev = thermal_cooling_device_register("thermal-devfreq-0", NULL, &mutt_cdev_ops);
	if (IS_ERR(x6837_devfreq_cdev))
		x6837_devfreq_cdev = NULL;

	x6837_flash_cdev = thermal_cooling_device_register("flashlight_cooler", NULL, &mutt_cdev_ops);
	if (IS_ERR(x6837_flash_cdev))
		x6837_flash_cdev = NULL;

	return 0;
}
static void __exit x6837_dbg_stubs_exit(void)
{
	if (x6837_flash_cdev)
		thermal_cooling_device_unregister(x6837_flash_cdev);
	if (x6837_devfreq_cdev)
		thermal_cooling_device_unregister(x6837_devfreq_cdev);
	if (x6837_mutt_cdev)
		thermal_cooling_device_unregister(x6837_mutt_cdev);
}
int tcpci_set_water_protection(void *tcpc, bool en) { return 0; }
EXPORT_SYMBOL(tcpci_set_water_protection);

void aee_kernel_RT_Monitor_api_factory(void) {}
EXPORT_SYMBOL(aee_kernel_RT_Monitor_api_factory);

int moto_chg_tcmd_register(void *client) { return 0; }
EXPORT_SYMBOL(moto_chg_tcmd_register);

int mtk_mmdvfs_debug_force_vcore_notify(u32 val) { return 0; }
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_force_vcore_notify);

void mtk_mmdvfs_debug_release_step0(void) {}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_release_step0);

void mml_pq_util_aee(const char *fmt, ...) {}
EXPORT_SYMBOL_GPL(mml_pq_util_aee);

/* CCMNI / TrustedMem / SecHeap optional RPS/FFA/GZ/pKVM hooks */
int set_rps_map(void *queue, unsigned long rps_value, int flag) { return 0; }
EXPORT_SYMBOL(set_rps_map);
int tmem_ffa_page_alloc(int type, void *buf_info, void *ops) { return -ENODEV; }
EXPORT_SYMBOL_GPL(tmem_ffa_page_alloc);
int tmem_ffa_page_free(int type, void *buf_info, void *ops) { return -ENODEV; }
EXPORT_SYMBOL_GPL(tmem_ffa_page_free);
int tmem_register_ffa_module(void *ops) { return 0; }
EXPORT_SYMBOL_GPL(tmem_register_ffa_module);
bool is_disable_map_sec(void) { return true; }
EXPORT_SYMBOL_GPL(is_disable_map_sec);
int tmem_type2sec_id(int type) { return 0; }
EXPORT_SYMBOL_GPL(tmem_type2sec_id);
unsigned long pkvm_el2_mod_call(int id, ...) { return -ENODEV; }
EXPORT_SYMBOL_GPL(pkvm_el2_mod_call);

int slbc_disable_dcc(bool disable) { return 0; }
EXPORT_SYMBOL_GPL(slbc_disable_dcc);
void mrdump_mini_add_extra_file(unsigned long vaddr, unsigned long paddr,
				unsigned long size, const char *name) {}
EXPORT_SYMBOL(mrdump_mini_add_extra_file);

/* VCODEC optional SMI-dbg / pf_ctrl hooks */
int mtk_smi_dbg_register_pwr_ctrl_cb(void *cb) { return 0; }
EXPORT_SYMBOL_GPL(mtk_smi_dbg_register_pwr_ctrl_cb);
int mtk_smi_dbg_unregister_pwr_ctrl_cb(void *cb) { return 0; }
EXPORT_SYMBOL_GPL(mtk_smi_dbg_unregister_pwr_ctrl_cb);
s32 smi_monitor_start(void *dev, u32 common_id, u32 *commonlarb_id, u32 *flags, int mon_type) { return 0; }
EXPORT_SYMBOL_GPL(smi_monitor_start);
s32 smi_monitor_stop(void *dev, u32 common_id, u32 *bw, int mon_type) { return 0; }
EXPORT_SYMBOL_GPL(smi_monitor_stop);
int mtk_set_pf_ctrl_enable(bool enable, unsigned int user) { return 0; }
EXPORT_SYMBOL_GPL(mtk_set_pf_ctrl_enable);
int mtk_get_pf_ctrl_enable(void) { return 0; }
EXPORT_SYMBOL_GPL(mtk_get_pf_ctrl_enable);

module_init(x6837_dbg_stubs_init);
module_exit(x6837_dbg_stubs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6789 6.12 optional debug/MML hook stubs");
