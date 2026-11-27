# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 xtrbasement
# MT6789 (X6837) out-of-tree kernel modules Kbuild
DM ?= $(src)
KS ?= $(srctree)

ccflags-y += -idirafter $(KS)/drivers/pinctrl \
             -I$(DM)/include -I$(DM)/drivers/clk/mediatek -I$(DM)/drivers/pinctrl/mediatek \
             -I$(DM)/drivers/soc/mediatek -I$(DM)/drivers/misc/mediatek/include \
             -I$(DM)/drivers/misc/mediatek/include/mt-plat \
             -I$(DM)/drivers/misc/mediatek/include/spm \
             -I$(DM)/drivers/misc/mediatek/usb/usb20 \
             -I$(DM)/drivers/misc/mediatek/usb/usb20/musb_main \
             -I$(KS)/drivers/usb/core -I$(KS)/drivers/usb/gadget \
             -I$(KS)/drivers/usb/gadget/function \
             -I$(DM)/drivers/ufs -I$(DM)/drivers/ufs/vendor \
             -I$(KS)/drivers/ufs -I$(KS)/drivers/ufs/core -I$(KS)/drivers/ufs/host \
             -I$(KS)/drivers/mmc/host \
             -I$(KS)/drivers/regulator \
             -DCONFIG_DEVICE_MODULES_EINT_MTK=1 \
             -DCONFIG_DEVICE_MODULES_PHY_MTK_TPHY=1 \
             -DCONFIG_MTK_MUSB_QMU_SUPPORT=1 \
             -DCONFIG_MTK_MUSB_DUAL_ROLE=1 \
             -DCONFIG_USB_MTK_HDRC=1 \
             -DCONFIG_USB_MTK_OTG=1 \
             -DCONFIG_MTK_MUSB_DRV_36BIT=1 \
             -DCONFIG_MTK_MUSB_QMU_PURE_ZLP_SUPPORT=1 \
             -DCONFIG_MTK_CMDQ_MBOX_EXT=1 \
             -DCONFIG_DEVICE_MODULES_MTK_SMI=1 \
             -DCONFIG_DEVICE_MODULES_MTK_IOMMU=1 \
             -DCONFIG_MTK_DVFSRC=1 \
             -Wno-unused-function -Wno-unused-variable -Wno-implicit-fallthrough

subdir-ccflags-y := $(ccflags-y)
NOSTDINC_FLAGS += -I$(DM)/include -I$(KS)/drivers/pinctrl

# 1. MT6789 Clocks
obj-m += clk-common.o
clk-common-y := drivers/clk/mediatek/clk-mtk.o \
                drivers/clk/mediatek/clk-pll.o \
                drivers/clk/mediatek/clk-gate.o \
                drivers/clk/mediatek/clk-mux.o \
                drivers/clk/mediatek/clkchk.o \
                drivers/clk/mediatek/clk-fmeter.o \
                drivers/clk/mediatek/mtk-pd-chk.o

obj-m += clk-mt6789.o
clk-mt6789-y := drivers/clk/mediatek/clk-mt6789.o

obj-m += clk-fmeter-mt6789.o
clk-fmeter-mt6789-y := drivers/clk/mediatek/clk-fmeter-mt6789.o

obj-m += clk-mt6789-bus.o
clk-mt6789-bus-y := drivers/clk/mediatek/clk-mt6789-bus.o

obj-m += clk-mt6789-i2c.o
clk-mt6789-i2c-y := drivers/clk/mediatek/clk-mt6789-i2c.o

obj-m += clk-mt6789-impen.o
clk-mt6789-impen-y := drivers/clk/mediatek/clk-mt6789-impen.o

obj-m += clk-mt6789-mmsys.o
clk-mt6789-mmsys-y := drivers/clk/mediatek/clk-mt6789-mmsys.o

obj-m += clk-mt6789-audiosys.o
clk-mt6789-audiosys-y := drivers/clk/mediatek/clk-mt6789-audiosys.o

obj-m += clk-mt6789-cam.o
clk-mt6789-cam-y := drivers/clk/mediatek/clk-mt6789-cam.o

obj-m += clk-mt6789-disp_dsc.o
clk-mt6789-disp_dsc-y := drivers/clk/mediatek/clk-mt6789-disp_dsc.o

obj-m += clk-mt6789-img.o
clk-mt6789-img-y := drivers/clk/mediatek/clk-mt6789-img.o

obj-m += clk-mt6789-mfgcfg.o
clk-mt6789-mfgcfg-y := drivers/clk/mediatek/clk-mt6789-mfgcfg.o

obj-m += clk-mt6789-vcodec.o
clk-mt6789-vcodec-y := drivers/clk/mediatek/clk-mt6789-vcodec.o

# 2. MT6789 Pinctrl
obj-m += pinctrl-mtk-v2.o
pinctrl-mtk-v2-y := drivers/pinctrl/mediatek/mtk-eint.o \
                    drivers/pinctrl/mediatek/pinctrl-mtk-common-v2.o \
                    drivers/pinctrl/mediatek/pinctrl-paris.o

obj-m += pinctrl-mt6789.o
pinctrl-mt6789-y := drivers/pinctrl/mediatek/pinctrl-mt6789.o

# 3. MT6789 SCPSYS Power Domains
obj-m += mtk-scpsys.o
mtk-scpsys-y := drivers/soc/mediatek/mtk-scpsys.o

obj-m += mtk-scpsys-mt6789.o
mtk-scpsys-mt6789-y := drivers/soc/mediatek/mtk-scpsys-mt6789.o

# 4. MT6789 MUSB (USB Gadget for Recovery ADB)
obj-m += musb_hdrc.o
musb_hdrc-y := drivers/misc/mediatek/usb/usb20/musb_core.o \
               drivers/misc/mediatek/usb/usb20/musb_virthub.o \
               drivers/misc/mediatek/usb/usb20/musb_host.o \
               drivers/misc/mediatek/usb/usb20/musb_gadget_ep0.o \
               drivers/misc/mediatek/usb/usb20/musb_gadget.o \
               drivers/misc/mediatek/usb/usb20/musb_trace.o \
               drivers/misc/mediatek/usb/usb20/musbhsdma.o \
               drivers/misc/mediatek/usb/usb20/usb20_host.o \
               drivers/misc/mediatek/usb/usb20/musb_dr.o \
               drivers/misc/mediatek/usb/usb20/musb_debugfs.o \
               drivers/misc/mediatek/usb/usb20/mtk_qmu.o \
               drivers/misc/mediatek/usb/usb20/musb_qmu.o

obj-m += musb_main.o
musb_main-y := drivers/misc/mediatek/usb/usb20/musb_main/musb.o

# 5. MT6789 USB/UFS PHY, PMIC Wrap, Watchdog, I2C, SPI
obj-m += phy-mtk-tphy.o
phy-mtk-tphy-y := drivers/phy/mediatek/phy-mtk-tphy.o

obj-m += phy-mtk-ufs.o
phy-mtk-ufs-y := drivers/phy/mediatek/phy-mtk-ufs.o

obj-m += mtk-pmic-wrap.o
mtk-pmic-wrap-y := drivers/soc/mediatek/mtk-pmic-wrap.o

obj-m += mtk_wdt.o
mtk_wdt-y := drivers/watchdog/mtk_wdt.o

# 6. MT6789 SPI (Touchscreen) + PMIC MT6366/MT6358 (UFS + Regulators)
obj-m += spi-mt65xx.o
spi-mt65xx-y := drivers/spi/spi-mt65xx.o

obj-m += mt6397.o
mt6397-y := drivers/mfd/mt6397-core.o \
            drivers/mfd/mt6397-irq.o \
            drivers/mfd/mt6358-irq.o

obj-m += mt6358-regulator.o
mt6358-regulator-y := drivers/regulator/mt6358-regulator.o

obj-m += mtk_disp_notify.o
mtk_disp_notify-y := drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.o

# 7. Physical Power & Volume Keys (for OrangeFox wake + screenshot)
obj-m += mtk-pmic-keys.o
mtk-pmic-keys-y := drivers/input/keyboard/mtk-pmic-keys.o

obj-m += mtk-kpd.o
mtk-kpd-y := drivers/input/keyboard/mtk-kpd.o

# 7b. RTC (Clock) + PMIC AUXADC + PMIC EFUSE (Battery Voltage/Temp) + I2C
obj-m += rtc-mt6397.o
rtc-mt6397-y := drivers/rtc/rtc-mt6397.o

obj-m += mt635x-efuse.o
mt635x-efuse-y := drivers/nvmem/mt635x-efuse.o

obj-m += mtk-devinfo.o
mtk-devinfo-y := drivers/nvmem/mtk-devinfo.o

obj-m += mt635x-auxadc.o
mt635x-auxadc-y := drivers/iio/adc/mt635x-auxadc.o

obj-m += mt6577_auxadc.o
mt6577_auxadc-y := drivers/iio/adc/mt6577_auxadc.o
CFLAGS_drivers/iio/adc/mt6577_auxadc.o := -DCONFIG_MTK_DEVINFO=1

ccflags-y += -I$(KS)/drivers/thermal
obj-m += thermal-generic-adc.o
thermal-generic-adc-y := drivers/thermal/thermal-generic-adc.o

obj-m += soc_temp_lvts.o
soc_temp_lvts-y := drivers/thermal/mediatek/soc_temp_lvts.o

obj-m += i2c-mt65xx.o
i2c-mt65xx-y := drivers/i2c/busses/i2c-mt65xx.o

# 7c. Display PWM + MTK PWM Backlight (0..4095) + MicroSD (11240000.mmc)
obj-m += pwm-mtk-disp.o
pwm-mtk-disp-y := drivers/pwm/pwm-mtk-disp.o

obj-m += leds-mtk.o
leds-mtk-y := drivers/leds/leds-mtk.o

obj-m += leds-mtk-pwm.o
leds-mtk-pwm-y := drivers/leds/leds-mtk-pwm.o

obj-m += mtk-sd.o
mtk-sd-y := drivers/mmc/host/mtk-sd.o

# 7d. Android Display Infrastructure: DMA-BUF System Heap + SMI + IOMMU + CMDQ GCE
ccflags-y += -I$(DM)/drivers/dma-buf/heaps \
             -I$(DM)/drivers/iommu \
             -I$(DM)/drivers/iommu/arm/arm-smmu-v3 \
             -I$(DM)/drivers/misc/mediatek/iommu \
             -I$(DM)/drivers/misc/mediatek/smi \
             -I$(DM)/drivers/misc/mediatek/mminfra \
             -I$(DM)/drivers/misc/mediatek/mmp \
             -I$(DM)/drivers/misc/mediatek/cmdq/mailbox \
             -I$(DM)/drivers/misc/mediatek/vcp/include \
             -I$(DM)/drivers/misc/mediatek/vcp/rv_v2 \
             -I$(DM)/drivers/misc/mediatek/geniezone

obj-m += mtk-dbg-stubs.o
mtk-dbg-stubs-y := x6837_mtk_dbg_stubs.o

obj-m += mtk-dvfsrc.o
mtk-dvfsrc-y := drivers/soc/mediatek/mtk-dvfsrc.o

obj-m += mtk-dvfsrc-regulator.o
mtk-dvfsrc-regulator-y := drivers/regulator/mtk-dvfsrc-regulator.o

obj-m += system_heap.o
system_heap-y := drivers/dma-buf/heaps/mtk-deferred-free-helper.o \
                 drivers/dma-buf/heaps/mtk_cache_pool.o \
                 drivers/dma-buf/heaps/mtk_dmabuf_monitor.o \
                 drivers/dma-buf/heaps/mtk_page_pool.o \
                 drivers/dma-buf/heaps/mtk_system_heap.o

obj-m += mtk-smi.o
mtk-smi-y := drivers/memory/mtk-smi.o

obj-m += mtk_iommu.o
mtk_iommu-y := drivers/iommu/mtk_iommu.o

obj-m += mtk-cmdq-drv-ext.o
mtk-cmdq-drv-ext-y := drivers/misc/mediatek/cmdq/mailbox/mtk-cmdq-mailbox-ext.o \
                      drivers/misc/mediatek/cmdq/mailbox/mtk-cmdq-helper-ext.o \
                      drivers/misc/mediatek/cmdq/mailbox/cmdq-util.o

obj-m += cmdq-platform-mt6789.o
cmdq-platform-mt6789-y := drivers/misc/mediatek/cmdq/mailbox/cmdq-platform-mt6789.o

# 7e. MTK DRM v2 (MT6789 Display Engine: /dev/dri/card0)
ccflags-y += -I$(DM)/drivers/gpu/drm/mediatek/mediatek_v2 \
             -I$(DM)/drivers/gpu/drm/mediatek/mml \
             -I$(DM)/drivers/gpu/drm/mediatek/dpc \
             -I$(DM)/drivers/misc/mediatek/dramc/mt6789 \
             -I$(DM)/drivers/misc/mediatek/spi_slave_drv \
             -I$(DM)/drivers/misc/mediatek/cmdq/bridge \
             -I$(DM)/drivers/misc/mediatek/cmdq \
             -I$(DM)/drivers/misc/mediatek/clkbuf/src \
             -I$(DM)/drivers/misc/mediatek/base/power/include \
             -I$(DM)/drivers/misc/mediatek/mmdvfs \
             -I$(DM)/drivers/misc/mediatek/pmic \
             -I$(DM)/drivers/devfreq \
             -I$(KS)/drivers/gpu/drm \
             -I$(KS)/drivers/gpu/drm/display \
             -I$(KS)/drivers/gpu/drm/mediatek \
             -I$(DM)/drivers/misc/mediatek/mtk-interconnect \
             -I$(DM)/drivers/interconnect/mediatek \
             -I$(DM)/sound/soc/mediatek/common \
             -I$(DM)/drivers/misc/mediatek/slbc \
             -I$(DM)/drivers/misc/mediatek/scp/include \
             -I$(DM)/drivers/misc/mediatek/tinysys_scmi \
             -I$(DM)/drivers/misc/mediatek/extcon \
             -DCONFIG_LEDS_MTK=1 \
             -DCONFIG_LEDS_MTK_MODULE=1 \
             -DDRM_PARTIAL_UPDATE=1 \
             -DDRM_RETRIGGER_ENABLE=1 \
             -DDISP_MDP_COLOR_ON=1 \
             -DDISP_STASH_ENABLE=1 \
             -Wno-dangling-else -Wno-unused-label -Wno-uninitialized \
             -Wno-shift-op-parentheses -Wno-format -Wno-missing-braces \
             -Wno-macro-redefined -Wno-frame-larger-than -Wno-switch \
             -Wno-misleading-indentation -Wno-int-conversion -Wno-visibility \
             -Wno-c23-extensions -Wno-implicit-function-declaration \
             -Wno-compare-distinct-pointer-types -Wno-logical-op-parentheses

obj-m += mtk_sync.o
mtk_sync-y := drivers/gpu/drm/mediatek/mediatek_v2/mtk_sync.o

obj-m += mtk_panel_ext.o
mtk_panel_ext-y := drivers/gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.o

# 7f. MT6789 GPU Platform (HAL + GPUFREQ v2_legacy)
ccflags-y += -I$(DM)/drivers/gpu/mediatek \
             -I$(DM)/drivers/gpu/mediatek/mt-plat \
             -I$(DM)/drivers/gpu/mediatek/gpufreq \
             -I$(DM)/drivers/gpu/mediatek/gpufreq/v2_legacy/include \
             -I$(DM)/drivers/gpu/mediatek/gpufreq/v2/include \
             -I$(DM)/drivers/gpu/mediatek/ged/include \
             -I$(DM)/drivers/gpu/mediatek/gpu_bm \
             -I$(DM)/drivers/gpu/mediatek/gpueb/include \
             -I$(DM)/drivers/thermal/mediatek \
             -I$(DM)/drivers/misc/mediatek/sspm/v3 \
             -I$(DM)/drivers/misc/mediatek/power_throttling \
             -I$(DM)/drivers/misc/mediatek/pbm \
             -I$(DM)/drivers/misc/mediatek/aee/aed \
             -DCONFIG_MTK_GPUFREQ_V2=1 \
             -DCONFIG_MTK_GPU_SUPPORT=1 \
             -DCONFIG_MTK_GPU_COMMON_DVFS_SUPPORT=1 \
             -DMTK_GPUFREQ_V2=1 \
             -DMTK_GED_KPI=1 \
             -DMTK_GPU_DVFS=1 \
             -DGED_DVFS_DEBUG_BUF=1 \
             -DCONFIG_MTK_GPU_APO_SUPPORT=1 \
             -I$(DM)/drivers/gpu/mediatek/ged/include/platform \
             -I$(DM)/drivers/gpu/mediatek/hal \
             -I$(DM)/drivers/gpu/mediatek/gpufreq/v1/include \
             -I$(DM)/drivers/gpu/mediatek/gpueb \
             -I$(DM)/drivers/misc/mediatek/sspm \
             -I$(DM)/drivers/misc/mediatek/sspm/v2 \
             -I$(DM)/drivers/misc/mediatek/qos \
             -I$(DM)/drivers/misc/mediatek/performance/fpsgo_v3

obj-m += mtk_gpu_hal.o
mtk_gpu_hal-y := drivers/gpu/mediatek/hal/mtk_gpu_utility.o

obj-m += mtk_gpufreq_wrapper_legacy.o
mtk_gpufreq_wrapper_legacy-y := \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpufreq_v2_legacy.o \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpufreq_history_legacy.o \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpufreq_debug_legacy.o

obj-m += mtk_gpufreq_mt6789.o
mtk_gpufreq_mt6789-y := \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpufreq_mt6789.o \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpuppm_legacy.o \
    drivers/gpu/mediatek/gpufreq/v2_legacy/gpufreq_history_mt6789.o

obj-m += ged.o
ged-y := \
    drivers/gpu/mediatek/ged/src/ged.o \
    drivers/gpu/mediatek/ged/src/ged_base.o \
    drivers/gpu/mediatek/ged/src/ged_main.o \
    drivers/gpu/mediatek/ged/src/ged_sysfs.o \
    drivers/gpu/mediatek/ged/src/ged_hal.o \
    drivers/gpu/mediatek/ged/src/ged_log.o \
    drivers/gpu/mediatek/ged/src/ged_bridge.o \
    drivers/gpu/mediatek/ged/src/ged_monitor_3D_fence.o \
    drivers/gpu/mediatek/ged/src/ged_notify_sw_vsync.o \
    drivers/gpu/mediatek/ged/src/ged_hashtable.o \
    drivers/gpu/mediatek/ged/src/ged_thread.o \
    drivers/gpu/mediatek/ged/src/ged_ge.o \
    drivers/gpu/mediatek/ged/src/ged_dvfs.o \
    drivers/gpu/mediatek/ged/src/ged_gpu_tuner.o \
    drivers/gpu/mediatek/ged/src/ged_kpi.o \
    drivers/gpu/mediatek/ged/src/ged_eb.o \
    drivers/gpu/mediatek/ged/src/ged_gpu_bm.o \
    drivers/gpu/mediatek/ged/src/ged_gpu_slc.o \
    drivers/gpu/mediatek/ged/src/ged_mali_event.o \
    drivers/gpu/mediatek/ged/src/ged_gpu_memsys.o \
    drivers/gpu/mediatek/ged/src/ged_gpufreq_v2.o \
    drivers/gpu/mediatek/ged/src/ged_dcs.o \
    drivers/gpu/mediatek/ged/src/ged_debugFS.o \
    drivers/gpu/mediatek/ged/src/ged_sync.o

obj-m += mediatek-drm.o
mediatek-drm-y := \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_drv.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_tdshp.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_color.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_ccorr.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_c3d.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_gamma.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_aal.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dither.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_chist.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_ovl.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_rdma.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_mdp_rdma.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_rsz.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_mdp_rsz.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_wdma.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_crtc.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_ddp.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_ddp_addon.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_ddp_comp.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_fb.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_gem.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_plane.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dsi.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_mipi_tx.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_writeback.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_fence.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_session.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dump.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_debug.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_layering_rule.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_layering_rule_base.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_rect.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_mmp.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_trace.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_helper.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_lowpower.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_postmask.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_assert.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_fbconsole.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_recovery.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_pmqos.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dsc.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_cm.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_spr.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_postalign.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_oddmr/mtk_disp_oddmr_parse_data.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_oddmr/mtk_disp_oddmr.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_drm_arr.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_merge.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dmdp_aal.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_fbconfig_kdebug.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_y2r0.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dlo_async.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dli_async.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_inlinerotate.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_mmlsys_bypass_mode.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_lease.o \
    drivers/gpu/drm/mediatek/mediatek_v2/platform/mtk_drm_6835.o \
    drivers/gpu/drm/mediatek/mediatek_v2/platform/mtk_drm_6789.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_pq_helper.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_vidle.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_r2y0.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_ovl_exdma.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_ovl_blender.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_ovl_outproc.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_vdisp_ao.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dp_intf.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dp.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dp_hal.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dp_debug.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_bdg.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_bwm.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dbi_count.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_relay.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dvo.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_dsi_lpc.o \
    drivers/gpu/drm/mediatek/mediatek_v2/mtk_disp_dbgtp.o

# 8. Trustonic MobiCore TEE (510a) for /data FBE Decryption & Keymaster
ccflags-y += -DNDEBUG -I$(DM)/drivers/tee/gud/510/MobiCoreDriver -I$(DM)/drivers/tee/gud/510/MobiCoreDriver/public -DCONFIG_TRUSTONIC_TEE_SUPPORT=1 -DCONFIG_TRUSTONIC_TEE_VERSION=\"510\"
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/nq.o := -fno-sanitize=bounds
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/mcp.o := -fno-sanitize=bounds
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/session.o := -fno-sanitize=bounds
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/user.o := -fno-sanitize=bounds
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/admin.o := -fno-sanitize=bounds
CFLAGS_drivers/tee/gud/510/MobiCoreDriver/client.o := -fno-sanitize=bounds
obj-m += mcDrvModule.o
mcDrvModule-y := \
    drivers/tee/gud/510/MobiCoreDriver/admin.o \
    drivers/tee/gud/510/MobiCoreDriver/client.o \
    drivers/tee/gud/510/MobiCoreDriver/clientlib.o \
    drivers/tee/gud/510/MobiCoreDriver/clock.o \
    drivers/tee/gud/510/MobiCoreDriver/fastcall.o \
    drivers/tee/gud/510/MobiCoreDriver/iwp.o \
    drivers/tee/gud/510/MobiCoreDriver/logging.o \
    drivers/tee/gud/510/MobiCoreDriver/main.o \
    drivers/tee/gud/510/MobiCoreDriver/mcp.o \
    drivers/tee/gud/510/MobiCoreDriver/mmu.o \
    drivers/tee/gud/510/MobiCoreDriver/nq.o \
    drivers/tee/gud/510/MobiCoreDriver/protocol.o \
    drivers/tee/gud/510/MobiCoreDriver/protocol_be.o \
    drivers/tee/gud/510/MobiCoreDriver/protocol_fe.o \
    drivers/tee/gud/510/MobiCoreDriver/session.o \
    drivers/tee/gud/510/MobiCoreDriver/teeclientapi.o \
    drivers/tee/gud/510/MobiCoreDriver/user.o \
    drivers/tee/gud/510/MobiCoreDriver/vlx_be.o \
    drivers/tee/gud/510/MobiCoreDriver/vlx_common.o \
    drivers/tee/gud/510/MobiCoreDriver/vlx_fe.o \
    drivers/tee/gud/510/MobiCoreDriver/xen_be.o \
    drivers/tee/gud/510/MobiCoreDriver/xen_common.o \
    drivers/tee/gud/510/MobiCoreDriver/xen_fe.o

# 9. Batch 1: Storage & DMA
ccflags-y += -I$(KS)/drivers/dma/mediatek \
             -I$(DM)/drivers/dma/mediatek

obj-m += ufs-mediatek-mod.o
ufs-mediatek-mod-y := \
    drivers/ufs/ufs-mediatek.o \
    drivers/ufs/ufs-mediatek-sysfs.o \
    drivers/ufs/ufs-mediatek-mimic.o \
    drivers/ufs/ufs-mediatek-mbrain.o

obj-m += mtk-cqdma.o
mtk-cqdma-y := drivers/dma/mediatek/mtk-cqdma.o

obj-m += mtk-uart-apdma.o
mtk-uart-apdma-y := drivers/dma/mediatek/mtk-uart-apdma.o
CFLAGS_drivers/dma/mediatek/mtk-uart-apdma.o := -DCONFIG_MTK_UARTHUB=1

# 10. Batch 2: Power / Charging / Gauge / Type-C
ccflags-y += -I$(DM)/drivers/power/supply \
             -I$(DM)/drivers/power/supply/ufcs \
             -I$(DM)/drivers/misc/mediatek/typec/tcpc \
             -I$(DM)/drivers/misc/mediatek/typec/tcpc/inc \
             -I$(DM)/drivers/mfd \
             -DCONFIG_RT_REGMAP=1 \
             -DCONFIG_USB_POWER_DELIVERY=1 \
             -DCONFIG_TCPC_CLASS=1

obj-m += pd_dbg_info.o
pd_dbg_info-y := drivers/misc/mediatek/typec/tcpc/pd_dbg_info.o

obj-m += tcpc_class.o
tcpc_class-y := \
    drivers/misc/mediatek/typec/tcpc/rt-regmap.o \
    drivers/misc/mediatek/typec/tcpc/tcpci.o \
    drivers/misc/mediatek/typec/tcpc/tcpci_alert.o \
    drivers/misc/mediatek/typec/tcpc/tcpci_core.o \
    drivers/misc/mediatek/typec/tcpc/tcpci_timer.o \
    drivers/misc/mediatek/typec/tcpc/tcpci_typec.o \
    drivers/misc/mediatek/typec/tcpc/tcpm.o \
    drivers/misc/mediatek/typec/tcpc/pd_core.o \
    drivers/misc/mediatek/typec/tcpc/pd_dpm_alt_mode_dp.o \
    drivers/misc/mediatek/typec/tcpc/pd_dpm_core.o \
    drivers/misc/mediatek/typec/tcpc/pd_dpm_pdo_select.o \
    drivers/misc/mediatek/typec/tcpc/pd_dpm_reaction.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_com.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_dbg.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_dfp.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_dr.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_drs.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_prs.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_snk.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_src.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_ufp.o \
    drivers/misc/mediatek/typec/tcpc/pd_policy_engine_vcs.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_com.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_dbg.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_drs.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_prs.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_snk.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_src.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_tcp.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_vcs.o \
    drivers/misc/mediatek/typec/tcpc/pd_process_evt_vdm.o \
    drivers/misc/mediatek/typec/tcpc/tcpci_event.o \
    drivers/misc/mediatek/typec/tcpc/pd_dpm_mmi_adapter.o

obj-m += tcpc_rt1711h.o
tcpc_rt1711h-y := drivers/misc/mediatek/typec/tcpc/tcpc_rt1711h.o

obj-m += rt_pd_manager.o
rt_pd_manager-y := drivers/misc/mediatek/typec/tcpc/rt_pd_manager.o

obj-m += charger_class.o
charger_class-y := drivers/power/supply/charger_class.o

obj-m += adapter_class.o
adapter_class-y := drivers/power/supply/adapter_class.o

obj-m += mtk_charger_algorithm_class.o
mtk_charger_algorithm_class-y := drivers/power/supply/mtk_charger_algorithm_class.o

obj-m += mtk_battery_manager.o
mtk_battery_manager-y := \
    drivers/power/supply/mtk-battery-manager.o \
    drivers/power/supply/mtk_battery_interface.o

obj-m += mtk_charger_framework.o
mtk_charger_framework-y := \
    drivers/power/supply/mtk_adapter_switch.o \
    drivers/power/supply/mtk_basic_charger.o \
    drivers/power/supply/mtk_charger.o \
    drivers/power/supply/mtk_charger_intf.o \
    drivers/power/supply/mtk_pulse_charger.o
CFLAGS_drivers/power/supply/mtk_charger.o += -DMTK_BASE=1
CFLAGS_drivers/power/supply/mtk_basic_charger.o += -DMTK_BASE=1
CFLAGS_drivers/power/supply/mtk_charger_intf.o += -DMTK_BASE=1
CFLAGS_drivers/power/supply/mtk_adapter_switch.o += -DMTK_BASE=1
CFLAGS_drivers/power/supply/mtk_pulse_charger.o += -DMTK_BASE=1

obj-m += mtk_chg_type_det.o
mtk_chg_type_det-y := drivers/power/supply/mtk_chg_type_det.o

obj-m += mtk_pd_adapter.o
mtk_pd_adapter-y := drivers/power/supply/mtk_pd_adapter.o
CFLAGS_drivers/power/supply/mtk_pd_adapter.o += -DMTK_BASE=1

obj-m += mtk_pd_charging.o
mtk_pd_charging-y := \
    drivers/power/supply/mtk_pd.o \
    drivers/power/supply/mtk_pd_hal.o
CFLAGS_drivers/power/supply/mtk_pd.o += -DMTK_BASE=1
CFLAGS_drivers/power/supply/mtk_pd_hal.o += -DMTK_BASE=1

obj-m += mt6358_battery.o
mt6358_battery-y := \
    drivers/power/supply/mtk_battery.o \
    drivers/power/supply/mtk_battery_algo.o \
    drivers/power/supply/mtk_battery_coulomb_service.o \
    drivers/power/supply/mtk_battery_daemon.o \
    drivers/power/supply/mtk_battery_interface.o \
    drivers/power/supply/mt6358-gauge.o

# 11. Batch 3: CPUFreq / DVFSRC / Thermal / Interconnect / LEDs / Vibrator / MML
ccflags-y += -I$(DM)/drivers/cpufreq \
             -I$(DM)/drivers/misc/mediatek/dvfsrc \
             -I$(DM)/drivers/leds \
             -I$(DM)/drivers/media/platform/mtk-vcodec \
             -DCONFIG_MTK_DVFSRC_HELPER=1 \
             -DCONFIG_INTERCONNECT_MTK_EXTENSION=1

obj-m += mediatek-cpufreq-hw.o
mediatek-cpufreq-hw-y := \
    drivers/cpufreq/mediatek-cpufreq-hw_main.o \
    drivers/cpufreq/mediatek-cpufreq-hw_fdvfs.o

obj-m += mtk-dvfsrc-devfreq.o
mtk-dvfsrc-devfreq-y := drivers/devfreq/mtk-dvfsrc-devfreq.o

obj-m += mtk-dvfsrc-helper.o
mtk-dvfsrc-helper-y := \
    drivers/misc/mediatek/dvfsrc/dvfsrc-helper.o \
    drivers/misc/mediatek/dvfsrc/dvfsrc-debug-v3.o \
    drivers/misc/mediatek/dvfsrc/dvfsrc-sysfs.o \
    drivers/misc/mediatek/dvfsrc/dvfsrc-debug-v2.o

obj-m += mtk-icc-core.o
mtk-icc-core-y := drivers/misc/mediatek/mtk-interconnect/core.o
CFLAGS_drivers/misc/mediatek/mtk-interconnect/core.o := -DCONFIG_INTERCONNECT_MTK_EXTENSION=1 -iquote $(DM)/drivers/misc/mediatek/mtk-interconnect

obj-m += thermal_interface.o
thermal_interface-y := drivers/thermal/mediatek/thermal_interface.o
CFLAGS_drivers/thermal/mediatek/thermal_interface.o := -DCONFIG_MTK_DVFSRC_HELPER=1 -DCONFIG_DEVICE_MODULES_DRM_MEDIATEK=1

obj-m += thermal_trace.o
thermal_trace-y := drivers/thermal/mediatek/thermal_trace.o
CFLAGS_drivers/thermal/mediatek/thermal_trace.o := -DCONFIG_MTK_MD_THERMAL=1 -iquote $(DM)/drivers/thermal/mediatek

obj-m += board_temp.o
board_temp-y := drivers/thermal/mediatek/board_temp.o

obj-m += pmic_temp.o
pmic_temp-y := drivers/thermal/mediatek/pmic_temp.o

obj-m += backlight_cooling.o
backlight_cooling-y := drivers/thermal/mediatek/backlight_cooling.o

obj-m += charger_cooling.o
charger_cooling-y := drivers/thermal/mediatek/charger_cooling.o

obj-m += md_cooling_all.o
md_cooling_all-y := \
    drivers/thermal/mediatek/md_cooling.o \
    drivers/thermal/mediatek/md_cooling_debugfs.o
CFLAGS_drivers/thermal/mediatek/md_cooling.o := -DCONFIG_MTK_MD_THERMAL=1 -iquote $(DM)/drivers/thermal/mediatek

obj-m += leds-mtk-disp.o
leds-mtk-disp-y := drivers/leds/leds-mtk-disp.o

obj-m += regulator-vibrator.o
regulator-vibrator-y := drivers/leds/regulator-vibrator.o

obj-m += mtk-mml.o
mtk-mml-y := \
    drivers/gpu/drm/mediatek/mml/DpTileScaler.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-aal.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-adaptor.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-buf.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-c3d.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-chist.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-color.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-core.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-dbgtp.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-dle-adaptor.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-dpc.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-driver.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-drm-adaptor.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-fg.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-fg-fw.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-hdr.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-m2m-adaptor.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-merge.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-monitor.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-mutex.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-pq-birsz.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-pq-birsz-fw.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-pq-core.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-pq-rdma.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-rdma.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-rrot.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-rsz.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-rsz-fw.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-scmi.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-sys.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-tcc.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-tdshp.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-tile.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-wdma.o \
    drivers/gpu/drm/mediatek/mml/mtk-mml-wrot.o \
    drivers/gpu/drm/mediatek/mml/tile_core.o \
    drivers/gpu/drm/mediatek/mml/tile_mdp_func.o

# 12. Batch 4: Co-processors / IPC / QoS / Clkbuf / FHCTL
ccflags-y += -I$(KS)/drivers/firmware/arm_scmi \
             -I$(KS)/drivers/rpmsg \
             -I$(DM)/drivers/misc/mediatek/mcupm/include \
             -I$(DM)/drivers/misc/mediatek/mcupm/v3/include \
             -I$(DM)/drivers/misc/mediatek/scp/rv \
             -I$(DM)/drivers/misc/mediatek/sspm/v3/include \
             -DMTK_SCMI=1

obj-m += mtk-mbox.o
mtk-mbox-y := drivers/soc/mediatek/mtk-mbox.o

obj-m += mtk_rpmsg_mbox.o
mtk_rpmsg_mbox-y := drivers/rpmsg/mtk_rpmsg_mbox.o

obj-m += mtk_tinysys_ipi.o
mtk_tinysys_ipi-y := drivers/soc/mediatek/mtk_tinysys_ipi.o

obj-m += tinysys-scmi.o
tinysys-scmi-y := \
    drivers/misc/mediatek/tinysys_scmi/tinysys-scmi-drv.o \
    drivers/misc/mediatek/tinysys_scmi/tinysys.o

obj-m += sspm_v3.o
sspm_v3-y := \
    drivers/misc/mediatek/sspm/v3/sspm_helper.o \
    drivers/misc/mediatek/sspm/v3/sspm_logger_impl.o \
    drivers/misc/mediatek/sspm/v3/sspm_plt.o \
    drivers/misc/mediatek/sspm/v3/sspm_reservedmem.o \
    drivers/misc/mediatek/sspm/v3/sspm_sysfs.o \
    drivers/misc/mediatek/sspm/v3/sspm_timesync.o

obj-m += mtk_qos.o
mtk_qos-y := \
    drivers/misc/mediatek/qos/mtk_qos_common.o \
    drivers/misc/mediatek/qos/mtk_qos_bound.o \
    drivers/misc/mediatek/qos/mtk_qos_sysfs.o \
    drivers/misc/mediatek/qos/mtk_qos_share.o \
    drivers/misc/mediatek/qos/qos-v6893.o \
    drivers/misc/mediatek/qos/mtk_qos_scmi.o

obj-m += mcupm.o
mcupm-y := \
    drivers/misc/mediatek/mcupm/v3/common/mcupm_helper.o \
    drivers/misc/mediatek/mcupm/v3/common/mcupm_timesync.o \
    drivers/misc/mediatek/mcupm/v3/common/mcupm_sysfs.o \
    drivers/misc/mediatek/mcupm/v3/mcupm_driver.o \
    drivers/misc/mediatek/mcupm/v3/mcupm_plt.o \
    drivers/misc/mediatek/mcupm/v3/mcupm_ipi.o

obj-m += scp.o
scp-y := \
    drivers/misc/mediatek/scp/rv/scp_helper.o \
    drivers/misc/mediatek/scp/rv/scp_excep.o \
    drivers/misc/mediatek/scp/rv/scp_wrapper_ipi.o \
    drivers/misc/mediatek/scp/rv/scp_irq.o \
    drivers/misc/mediatek/scp/rv/scp_logger.o \
    drivers/misc/mediatek/scp/rv/scp_l1c.o \
    drivers/misc/mediatek/scp/rv/scp_scpctl.o \
    drivers/misc/mediatek/scp/rv/scp_chre_manager.o \
    drivers/misc/mediatek/scp/rv/scp_feature_table.o \
    drivers/misc/mediatek/scp/rv/scp_awake.o \
    drivers/misc/mediatek/scp/rv/scp_dvfs.o \
    drivers/misc/mediatek/scp/rv/scp_hwvoter_dbg.o \
    drivers/misc/mediatek/scp/rv/scp_low_pwr_dbg.o \
    drivers/misc/mediatek/scp/rv/scp_mbrain_dbg.o \
    drivers/misc/mediatek/scp/rv/scp_tmon_dbg.o \
    drivers/misc/mediatek/scp/rv/sap.o
CFLAGS_drivers/misc/mediatek/scp/rv/sap.o := -DCONFIG_MTK_TINYSYS_SAP_SUPPORT=1
CFLAGS_drivers/misc/mediatek/scp/rv/scp_dvfs.o := -DCONFIG_DEVICE_MODULES_MFD_MT6397=1

obj-m += clkbuf.o
clkbuf-y := \
    drivers/misc/mediatek/clkbuf/src/clkbuf-ctrl.o \
    drivers/misc/mediatek/clkbuf/src/clkbuf-pmic.o \
    drivers/misc/mediatek/clkbuf/src/srclken.o \
    drivers/misc/mediatek/clkbuf/src/clkbuf-pmif.o \
    drivers/misc/mediatek/clkbuf/src/platform-hdlr.o \
    drivers/misc/mediatek/clkbuf/src/clkbuf-debug.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6357.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6358.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6358-tb.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6359p.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6366.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6366-tb.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6685.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6685-tb.o \
    drivers/misc/mediatek/clkbuf/src/dcxo-6687.o \
    drivers/misc/mediatek/clkbuf/src/pmif-data-v1.o \
    drivers/misc/mediatek/clkbuf/src/src-data-v1.o \
    drivers/misc/mediatek/clkbuf/src/src-data-v2.o \
    drivers/misc/mediatek/clkbuf/src/src-data-v3.o

obj-m += fhctl.o
fhctl-y := \
    drivers/clk/mediatek/clk-fhctl-ap.o \
    drivers/clk/mediatek/clk-fhctl-debug.o \
    drivers/clk/mediatek/clk-fhctl-pll.o \
    drivers/clk/mediatek/clk-fhctl.o \
    drivers/clk/mediatek/clk-fhctl-mcupm.o

# 13. Batch 5: Audio MT6789 / MT6366
ccflags-y += -I$(DM)/sound/soc/mediatek/mt6789 \
             -I$(DM)/sound/soc/codecs \
             -I$(DM)/sound/soc/codecs/richtek \
             -I$(DM)/sound/soc/codecs/tfa98xx \
             -I$(DM)/sound/soc/mediatek/ultrasound/ultra_common \
             -I$(DM)/sound/soc/mediatek/ultrasound/ultra_scp

obj-m += mtk-afe-external.o
mtk-afe-external-y := sound/soc/mediatek/common/mtk-afe-external.o

obj-m += mtk-sp-spk-amp.o
mtk-sp-spk-amp-y := sound/soc/mediatek/common/mtk-sp-spk-amp.o

obj-m += snd-soc-mtk-common.o
snd-soc-mtk-common-y := \
    sound/soc/mediatek/common/mtk-afe-platform-driver.o \
    sound/soc/mediatek/common/mtk-afe-fe-dai.o \
    sound/soc/mediatek/common/mtk-mmap-ion.o \
    sound/soc/mediatek/common/mtk-usb-offload-ops.o \
    sound/soc/mediatek/common/mtk-sram-manager.o \
    sound/soc/mediatek/common/mtk-sp-pcm-ops.o \
    sound/soc/mediatek/common/mtk-afe-debug.o

obj-m += snd-soc-mt6789-afe.o
snd-soc-mt6789-afe-y := \
    sound/soc/mediatek/mt6789/mt6789-afe-pcm.o \
    sound/soc/mediatek/mt6789/mt6789-afe-clk.o \
    sound/soc/mediatek/mt6789/mt6789-afe-gpio.o \
    sound/soc/mediatek/mt6789/mt6789-dai-adda.o \
    sound/soc/mediatek/mt6789/mt6789-afe-control.o \
    sound/soc/mediatek/mt6789/mt6789-dai-i2s.o \
    sound/soc/mediatek/mt6789/mt6789-dai-hw-gain.o \
    sound/soc/mediatek/mt6789/mt6789-dai-src.o \
    sound/soc/mediatek/mt6789/mt6789-dai-pcm.o \
    sound/soc/mediatek/mt6789/mt6789-dai-hostless.o \
    sound/soc/mediatek/mt6789/mt6789-misc-control.o

obj-m += mt6358-accdet.o
mt6358-accdet-y := sound/soc/codecs/mt6358-accdet.o

obj-m += snd-soc-mt6366.o
snd-soc-mt6366-y := sound/soc/codecs/mt6358.o

obj-m += mt6789-mt6366.o
mt6789-mt6366-y := sound/soc/mediatek/mt6789/mt6789-mt6366.o

# 14. Batch 6: Flashlight / Sensors / Modem / TrustedMem / SecHeap / BTIF
ccflags-y += -I$(DM)/drivers/misc/mediatek/flashlight \
             -I$(DM)/drivers/misc/mediatek/sensor/2.0/core \
             -I$(DM)/drivers/misc/mediatek/sensor/2.0/sensorhub \
             -I$(DM)/drivers/misc/mediatek/ccci_util \
             -I$(DM)/drivers/misc/mediatek/ccmni \
             -I$(DM)/drivers/misc/mediatek/eccci/inc \
             -I$(DM)/drivers/misc/mediatek/c2k_ccci \
             -I$(DM)/drivers/misc/mediatek/btif/common/inc \
             -I$(DM)/drivers/misc/mediatek/btif/common/plat_inc \
             -I$(DM)/drivers/misc/mediatek/trusted_mem \
             -I$(DM)/drivers/misc/mediatek/trusted_mem/ssheap \
             -I$(DM)/drivers/misc/mediatek/trusted_mem/ssmr \
             -I$(DM)/drivers/misc/mediatek/trusted_mem/tmemperf \
             -DBTIF_DBG_SUPPORT=0 \
             -DTEE_DEVICES_SUPPORT=1 \
             -DCONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM=1 \
             -DCONFIG_MTK_SECURE_MEM_SUPPORT=1

obj-m += flashlight.o
flashlight-y := \
    drivers/misc/mediatek/flashlight/flashlight-core.o \
    drivers/misc/mediatek/flashlight/flashlight-device.o

obj-m += hf_manager.o
hf_manager-y := drivers/misc/mediatek/sensor/2.0/core/hf_manager.o

obj-m += sensorhub.o
sensorhub-y := \
    drivers/misc/mediatek/sensor/2.0/sensorhub/tiny_crc8.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/tiny_crc32.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/ready.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/ipi_comm.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/sensor_comm.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/sensor_list.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/transceiver.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/share_memory.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/timesync.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/debug.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/custom_cmd.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/sensor_freq.o \
    drivers/misc/mediatek/sensor/2.0/sensorhub/sap_custom_cmd.o
CFLAGS_drivers/misc/mediatek/sensor/2.0/sensorhub/sap_custom_cmd.o := -DCONFIG_MTK_TINYSYS_SAP_SUPPORT=1

obj-m += ccci_util_lib.o
ccci_util_lib-y := \
    drivers/misc/mediatek/ccci_util/ccci_private_log.o \
    drivers/misc/mediatek/ccci_util/ccci_util_boot_args.o \
    drivers/misc/mediatek/ccci_util/ccci_util_broadcast.o \
    drivers/misc/mediatek/ccci_util/ccci_util_dummy.o \
    drivers/misc/mediatek/ccci_util/ccci_util_img_parsing.o \
    drivers/misc/mediatek/ccci_util/ccci_util_ld_md_errno.o \
    drivers/misc/mediatek/ccci_util/ccci_util_lib_fo.o \
    drivers/misc/mediatek/ccci_util/ccci_util_lib_load_img.o \
    drivers/misc/mediatek/ccci_util/ccci_util_lib_main.o \
    drivers/misc/mediatek/ccci_util/ccci_util_lib_reserved_mem.o \
    drivers/misc/mediatek/ccci_util/ccci_util_lib_sys.o \
    drivers/misc/mediatek/ccci_util/ccci_util_md_mem.o \
    drivers/misc/mediatek/ccci_util/ccci_util_md_rat.o \
    drivers/misc/mediatek/ccci_util/ccci_util_pin_broadcast.o \
    drivers/misc/mediatek/ccci_util/ccci_util_sib.o

obj-m += ccmni.o
ccmni-y := drivers/misc/mediatek/ccmni/ccmni.o
CFLAGS_drivers/misc/mediatek/ccmni/ccmni.o := -I$(DM)/drivers/misc/mediatek/rps

obj-m += btif_drv.o
btif_drv-y := \
    drivers/misc/mediatek/btif/common/mtk_btif.o \
    drivers/misc/mediatek/btif/common/mtk_btif_exp.o \
    drivers/misc/mediatek/btif/common/btif_dma_plat.o \
    drivers/misc/mediatek/btif/common/btif_plat.o

obj-m += trusted_mem.o
trusted_mem-y := \
    drivers/misc/mediatek/trusted_mem/alloc_api.o \
    drivers/misc/mediatek/trusted_mem/dev_mgr.o \
    drivers/misc/mediatek/trusted_mem/entry.o \
    drivers/misc/mediatek/trusted_mem/peer_mgr.o \
    drivers/misc/mediatek/trusted_mem/proc.o \
    drivers/misc/mediatek/trusted_mem/region_mgr.o \
    drivers/misc/mediatek/trusted_mem/ssheap_proc.o \
    drivers/misc/mediatek/trusted_mem/ssheap/ssheap_lib.o \
    drivers/misc/mediatek/trusted_mem/ssmr_mgr.o \
    drivers/misc/mediatek/trusted_mem/ssmr/memory_ssmr.o \
    drivers/misc/mediatek/trusted_mem/tee_devices.o \
    drivers/misc/mediatek/trusted_mem/tee_impl/tee_invoke.o \
    drivers/misc/mediatek/trusted_mem/tee_impl/tee_ops.o
CFLAGS_drivers/misc/mediatek/trusted_mem/tee_impl/tee_ops.o := -I$(DM)/drivers/tee/gud/510/MobiCoreDriver/public/GP

obj-m += mtk_sec_heap.o
mtk_sec_heap-y := \
    drivers/dma-buf/heaps/mtk_sec_heap_main.o \
    drivers/dma-buf/heaps/mtk_tmem_interop.o
CFLAGS_drivers/dma-buf/heaps/mtk_sec_heap_main.o := -DCONFIG_MTK_PROT_MEM_SSHEAP_V2=1
CFLAGS_drivers/dma-buf/heaps/mtk_tmem_interop.o := -DCONFIG_MTK_PROT_MEM_SSHEAP_V2=1 -include $(DM)/drivers/misc/mediatek/include/pkvm_mgmt/pkvm_mgmt.h

# 15. MT6375 Sub-PMIC / ADC / Charger / TCPC (X6837 Battery & Charging)
obj-m += mt6375.o
mt6375-y := drivers/mfd/mt6375.o

obj-m += mt6375-adc.o
mt6375-adc-y := drivers/iio/adc/mt6375-adc.o

obj-m += mt6375-auxadc.o
mt6375-auxadc-y := drivers/iio/adc/mt6375-auxadc.o

obj-m += mt6375-charger.o
mt6375-charger-y := drivers/power/supply/mt6375-charger.o
CFLAGS_drivers/power/supply/mt6375-charger.o := \
    -I$(DM)/drivers/power/supply \
    -I$(DM)/drivers/misc/mediatek/typec/tcpc/inc \
    -I$(DM)/drivers/misc/mediatek/power_throttling

obj-m += tcpc_mt6375.o
tcpc_mt6375-y := drivers/misc/mediatek/typec/tcpc/tcpc_mt6375.o
CFLAGS_drivers/misc/mediatek/typec/tcpc/tcpc_mt6375.o := \
    -I$(DM)/drivers/misc/mediatek/typec/tcpc/inc \
    -I$(DM)/drivers/misc/mediatek/typec/tcpc \
    -I$(DM)/drivers/power/supply

# 16. ECCCI Modem / Baseband / RIL (MT6789)
ccflags-y += -DCCCI_KMODULE_ENABLE \
             -I$(DM)/drivers/misc/mediatek/eccci \
             -I$(DM)/drivers/misc/mediatek/eccci/inc \
             -I$(DM)/drivers/misc/mediatek/eccci/fsm \
             -I$(DM)/drivers/misc/mediatek/eccci/hif \
             -I$(DM)/drivers/misc/mediatek/eccci/port \
             -I$(DM)/drivers/misc/mediatek/eccci/udc \
             -I$(DM)/drivers/misc/mediatek/eccci/udc/udc_lib_inc \
             -I$(DM)/drivers/misc/mediatek/conn_md/include \
             -I$(DM)/drivers/misc/mediatek/mdpm \
             -I$(DM)/drivers/misc/mediatek/pbm \
             -I$(DM)/drivers/misc/mediatek/slbc \
             -I$(DM)/drivers/misc/mediatek/memory-amms \
             -I$(DM)/drivers/misc/mediatek/scp/include \
             -I$(DM)/drivers/misc/mediatek/scp/rv

obj-m += mtk_mdpm.o
mtk_mdpm-y := drivers/misc/mediatek/mdpm/mtk_mdpm.o
CFLAGS_drivers/misc/mediatek/mdpm/mtk_mdpm.o += -DCONFIG_MTK_ECCCI_DRIVER=1

obj-m += ccci_auxadc.o
ccci_auxadc-y := drivers/misc/mediatek/eccci/ccci_auxadc.o

obj-m += ccci_ccif.o
ccci_ccif-y := \
    drivers/misc/mediatek/eccci/hif/ccci_hif_ccif.o \
    drivers/misc/mediatek/eccci/hif/ccci_ringbuf.o

obj-m += ccci_dpmaif.o
ccci_dpmaif-y := \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_bat.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_com.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_debug.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_drv_com.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_drv_v1.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_drv_v2.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_drv_v3.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_page_pool.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_resv_mem.o \
    drivers/misc/mediatek/eccci/hif/ccci_dpmaif_v1_6789.o \
    drivers/misc/mediatek/eccci/hif/md_spd_dvfs_fn.o \
    drivers/misc/mediatek/eccci/hif/md_spd_dvfs_method.o \
    drivers/misc/mediatek/eccci/hif/net_pool.o \
    drivers/misc/mediatek/eccci/hif/net_speed.o

obj-m += ccci_fsm_scp.o
ccci_fsm_scp-y := drivers/misc/mediatek/eccci/fsm/ccci_fsm_scp_c.o

obj-m += ccci_md_all.o
ccci_md_all-y := \
    drivers/misc/mediatek/eccci/ccci_bm.o \
    drivers/misc/mediatek/eccci/ccci_core.o \
    drivers/misc/mediatek/eccci/ccci_modem.o \
    drivers/misc/mediatek/eccci/port/ccci_mbrain.o \
    drivers/misc/mediatek/eccci/port/ccci_swtp.o \
    drivers/misc/mediatek/eccci/port/port_cfg.o \
    drivers/misc/mediatek/eccci/port/port_char.o \
    drivers/misc/mediatek/eccci/port/port_ctlmsg.o \
    drivers/misc/mediatek/eccci/port/port_ipc.o \
    drivers/misc/mediatek/eccci/port/port_kernel_interface.o \
    drivers/misc/mediatek/eccci/port/port_poller.o \
    drivers/misc/mediatek/eccci/port/port_proxy.o \
    drivers/misc/mediatek/eccci/port/port_rpc.o \
    drivers/misc/mediatek/eccci/port/port_smem.o \
    drivers/misc/mediatek/eccci/port/port_sysmsg.o \
    drivers/misc/mediatek/eccci/port/port_udc.o \
    drivers/misc/mediatek/eccci/udc/udc.o \
    drivers/misc/mediatek/eccci/hif/ccci_hif.o \
    drivers/misc/mediatek/eccci/fsm/ap_md_mem.o \
    drivers/misc/mediatek/eccci/fsm/ap_md_reg_dump.o \
    drivers/misc/mediatek/eccci/fsm/ap_md_reg_dump_gen93.o \
    drivers/misc/mediatek/eccci/fsm/ap_md_reg_dump_gen95.o \
    drivers/misc/mediatek/eccci/fsm/ap_md_reg_dump_gen97.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm_ee.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm_ioctl.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm_monitor.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm_poller.o \
    drivers/misc/mediatek/eccci/fsm/ccci_fsm_sys.o \
    drivers/misc/mediatek/eccci/fsm/mdee_dumper_v1.o \
    drivers/misc/mediatek/eccci/fsm/mdee_dumper_v2.o \
    drivers/misc/mediatek/eccci/fsm/mdee_dumper_v3.o \
    drivers/misc/mediatek/eccci/fsm/mdee_dumper_v5.o \
    drivers/misc/mediatek/eccci/fsm/mdee_dumper_v6.o \
    drivers/misc/mediatek/eccci/fsm/md_sys1_platform.o \
    drivers/misc/mediatek/eccci/fsm/modem_sys1.o
CFLAGS_drivers/misc/mediatek/eccci/port/port_sysmsg.o := -DCONFIG_DEVICE_MODULES_DRM_MEDIATEK=1 -I$(DM)/drivers/gpu/drm/mediatek/mediatek_v2

# 17. Media C2 Video Codec (MTK VCU + VCODEC DEC/ENC)
VCODEC_CFLAGS := -I$(DM)/drivers/media/platform/mtk-vcodec \
                 -I$(DM)/drivers/media/platform/mtk-vcodec/vdec \
                 -I$(DM)/drivers/media/platform/mtk-vcodec/venc \
                 -I$(DM)/drivers/media/platform/mtk-vcodec/vdvfs_vqos \
                 -I$(DM)/drivers/media/platform/mtk-vcu \
                 -I$(DM)/drivers/misc/mediatek/smi \
                 -I$(DM)/drivers/misc/mediatek/slbc \
                 -I$(DM)/drivers/misc/mediatek/cpufreq_lite \
                 -I$(DM)/drivers/misc/mediatek/vcp/rv \
                 -I$(DM)/drivers/misc/mediatek/vcp/include \
                 -I$(DM)/drivers/misc/mediatek/iommu \
                 -I$(DM)/drivers/misc/mediatek/include/mt-plat \
                 -DCONFIG_VIDEO_MEDIATEK_VCU=1 \
                 -DCONFIG_VIDEO_MEDIATEK_VCODEC=1 \
                 -include $(DM)/include/dt-bindings/memory/mtk-memory-port.h

obj-m += mtk-vcu.o
mtk-vcu-y := \
    drivers/media/platform/mtk-vcu/mtk_vcu.o \
    drivers/media/platform/mtk-vcu/mtk_vcodec_mem.o
CFLAGS_drivers/media/platform/mtk-vcu/mtk_vcu.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcu/mtk_vcodec_mem.o := $(VCODEC_CFLAGS)

obj-m += mtk-vcodec-common.o
mtk-vcodec-common-y := \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_util.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_intr.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_fence.o
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_util.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_intr.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_fence.o := $(VCODEC_CFLAGS)

obj-m += mtk-vcodec-dec.o
mtk-vcodec-dec-y := \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_drv.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_dec.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_pm.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_slc.o \
    drivers/media/platform/mtk-vcodec/vdec_drv_if.o \
    drivers/media/platform/mtk-vcodec/vdvfs_vqos/mtk_vcodec_dec_pm_plat.o \
    drivers/media/platform/mtk-vcodec/vdvfs_vqos/vcodec_dvfs.o \
    drivers/media/platform/mtk-vcodec/vdec/vdec_common_if.o \
    drivers/media/platform/mtk-vcodec/vdec/vdec_vcu_if.o
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_drv.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_dec.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_pm.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_dec_slc.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdec_drv_if.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdvfs_vqos/mtk_vcodec_dec_pm_plat.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdvfs_vqos/vcodec_dvfs.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdec/vdec_common_if.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdec/vdec_vcu_if.o := $(VCODEC_CFLAGS)

obj-m += mtk-vcodec-enc.o
mtk-vcodec-enc-y := \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_enc_drv.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_enc.o \
    drivers/media/platform/mtk-vcodec/mtk_vcodec_enc_pm.o \
    drivers/media/platform/mtk-vcodec/venc_drv_if.o \
    drivers/media/platform/mtk-vcodec/vdvfs_vqos/mtk_vcodec_enc_pm_plat.o \
    drivers/media/platform/mtk-vcodec/vdvfs_vqos/vcodec_dvfs.o \
    drivers/media/platform/mtk-vcodec/venc/venc_common_if.o \
    drivers/media/platform/mtk-vcodec/venc/venc_vcu_if.o
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_enc_drv.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_enc.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/mtk_vcodec_enc_pm.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/venc_drv_if.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/vdvfs_vqos/mtk_vcodec_enc_pm_plat.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/venc/venc_common_if.o := $(VCODEC_CFLAGS)
CFLAGS_drivers/media/platform/mtk-vcodec/venc/venc_vcu_if.o := $(VCODEC_CFLAGS)

# 18. VDEC-FMT + MDP (MT6789) for Media C2 YUV->RGB / Format Conversion
VDEC_FMT_CFLAGS := -I$(DM)/drivers/misc/mediatek/mtk-interconnect \
                   -I$(DM)/drivers/misc/mediatek/cmdq/mailbox \
                   -I$(DM)/drivers/misc/mediatek/include \
                   -I$(DM)/drivers/misc/mediatek/iommu \
                   -I$(DM)/drivers/iommu
obj-m += vdec-fmt.o
vdec-fmt-y := \
    drivers/misc/mediatek/vdec_fmt/vdec_fmt_driver.o \
    drivers/misc/mediatek/vdec_fmt/vdec_fmt_pm.o \
    drivers/misc/mediatek/vdec_fmt/vdec_fmt_dmabuf.o \
    drivers/misc/mediatek/vdec_fmt/vdec_fmt_sync.o
CFLAGS_drivers/misc/mediatek/vdec_fmt/vdec_fmt_driver.o := $(VDEC_FMT_CFLAGS)
CFLAGS_drivers/misc/mediatek/vdec_fmt/vdec_fmt_pm.o := $(VDEC_FMT_CFLAGS)
CFLAGS_drivers/misc/mediatek/vdec_fmt/vdec_fmt_dmabuf.o := $(VDEC_FMT_CFLAGS)
CFLAGS_drivers/misc/mediatek/vdec_fmt/vdec_fmt_sync.o := $(VDEC_FMT_CFLAGS)

MDP_CFLAGS := -I$(DM)/drivers/misc/mediatek/mdp \
              -I$(DM)/drivers/misc/mediatek/smi \
              -I$(DM)/drivers/misc/mediatek/mmdvfs \
              -I$(DM)/drivers/misc/mediatek/mtk-interconnect \
              -I$(DM)/drivers/misc/mediatek/include \
              -I$(DM)/drivers/misc/mediatek/mmp \
              -I$(DM)/drivers/iommu \
              -I$(DM)/drivers/misc/mediatek/iommu \
              -I$(DM)/drivers/gpu/drm/mediatek/dpc \
              -I$(DM)/drivers/misc/mediatek/cmdq/mailbox \
              -I$(DM)/drivers/dma-buf/heaps \
              -DCMDQ_LARGE_MAX_FIRSTERROR_BUFFER \
              -include $(DM)/include/dt-bindings/memory/mtk-memory-port.h
obj-m += mdp_drv_mt6789.o
mdp_drv_mt6789-y := \
    drivers/misc/mediatek/mdp/mdp_mt6789.o \
    drivers/misc/mediatek/mdp/mdp_driver.o \
    drivers/misc/mediatek/mdp/mdp_ioctl_ex.o \
    drivers/misc/mediatek/mdp/mdp_common.o \
    drivers/misc/mediatek/mdp/cmdq_record.o \
    drivers/misc/mediatek/mdp/cmdq_virtual.o \
    drivers/misc/mediatek/mdp/cmdq_device.o \
    drivers/misc/mediatek/mdp/mdp_event_common.o \
    drivers/misc/mediatek/mdp/cmdq_subsys_common.o \
    drivers/misc/mediatek/mdp/cmdq_helper_ext.o \
    drivers/misc/mediatek/mdp/cmdq_helper_inf.o \
    drivers/misc/mediatek/mdp/cmdq_mmp.o \
    drivers/misc/mediatek/mdp/mdp_fence.o \
    drivers/misc/mediatek/mdp/mdp_dpc.o
CFLAGS_drivers/misc/mediatek/mdp/mdp_mt6789.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_driver.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_ioctl_ex.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_common.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_record.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_virtual.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_device.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_event_common.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_subsys_common.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_helper_ext.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_helper_inf.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/cmdq_mmp.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_fence.o := $(MDP_CFLAGS)
CFLAGS_drivers/misc/mediatek/mdp/mdp_dpc.o := $(MDP_CFLAGS)
obj-m += drivers/misc/mediatek/typec/tcpc/tcpci_late_sync.o
