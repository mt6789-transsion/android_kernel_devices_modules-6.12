// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Transsion Holdings
 *
 * Transsion fingerprint driver.
 */

#define pr_fmt(fmt) "[Tran_FP] " fmt

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netlink.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <net/sock.h>

#include "mtk_disp_notify.h"

/* Exported by spi-mt65xx.ko */
extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

/* Netlink protocol & Goodix events */
#define TRAN_FP_NETLINK_DEFAULT		30
#define GF_NET_EVENT_IRQ		1
#define GF_NET_EVENT_FB_BLACK		2
#define GF_NET_EVENT_FB_UNBLACK		3

/* Goodix IOCTLs ('g' = 0x67) */
#define GF_IOC_MAGIC			'g'

struct gf_key {
	unsigned int key;
	int value;
};

struct gf_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};

#define GF_IOC_INIT			_IOR(GF_IOC_MAGIC, 0, u8)
#define GF_IOC_EXIT			_IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET			_IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ		_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ		_IO(GF_IOC_MAGIC, 4)
#define GF_IOC_ENABLE_SPI_CLK		_IOW(GF_IOC_MAGIC, 5, u32)
#define GF_IOC_DISABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 6)
#define GF_IOC_ENABLE_POWER		_IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER		_IO(GF_IOC_MAGIC, 8)
#define GF_IOC_INPUT_KEY_EVENT		_IOW(GF_IOC_MAGIC, 9, struct gf_key)
#define GF_IOC_ENTER_SLEEP_MODE		_IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO		_IOR(GF_IOC_MAGIC, 11, u8)
#define GF_IOC_REMOVE			_IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO		_IOW(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)
#define GF_IOC_NAV_EVENT		_IOW(GF_IOC_MAGIC, 14, u32)

/* Chipone IOCTLs (0xf0) */
#define FPSENSOR_IOC_MAGIC			0xf0
#define FPSENSOR_IOC_INIT			_IOWR(FPSENSOR_IOC_MAGIC, 0, u32)
#define FPSENSOR_IOC_EXIT			_IOWR(FPSENSOR_IOC_MAGIC, 1, u32)
#define FPSENSOR_IOC_RESET			_IOWR(FPSENSOR_IOC_MAGIC, 2, u32)
#define FPSENSOR_IOC_ENABLE_IRQ			_IOWR(FPSENSOR_IOC_MAGIC, 3, u32)
#define FPSENSOR_IOC_DISABLE_IRQ		_IOWR(FPSENSOR_IOC_MAGIC, 4, u32)
#define FPSENSOR_IOC_GET_INT_VAL		_IOWR(FPSENSOR_IOC_MAGIC, 5, u32)
#define FPSENSOR_IOC_DISABLE_SPI_CLK		_IOWR(FPSENSOR_IOC_MAGIC, 6, u32)
#define FPSENSOR_IOC_ENABLE_SPI_CLK		_IOWR(FPSENSOR_IOC_MAGIC, 7, u32)
#define FPSENSOR_IOC_ENABLE_POWER		_IOWR(FPSENSOR_IOC_MAGIC, 8, u32)
#define FPSENSOR_IOC_DISABLE_POWER		_IOWR(FPSENSOR_IOC_MAGIC, 9, u32)
#define FPSENSOR_IOC_REMOVE			_IOWR(FPSENSOR_IOC_MAGIC, 12, u32)
#define FPSENSOR_IOC_CANCEL_WAIT		_IOWR(FPSENSOR_IOC_MAGIC, 13, u32)
#define FPSENSOR_IOC_GET_FP_STATUS		_IOWR(FPSENSOR_IOC_MAGIC, 19, u32)
#define FPSENSOR_IOC_ENABLE_REPORT_BLANKON	_IOWR(FPSENSOR_IOC_MAGIC, 21, u32)
#define FPSENSOR_IOC_UPDATE_DRIVER_SN		_IOWR(FPSENSOR_IOC_MAGIC, 22, u32)

/* Navigation / key codes */
enum tran_nav_event {
	TRAN_NAV_NONE = 0,
	TRAN_NAV_FINGER_UP = 1,
	TRAN_NAV_FINGER_DOWN = 2,
	TRAN_NAV_UP = 3,
	TRAN_NAV_DOWN = 4,
	TRAN_NAV_LEFT = 5,
	TRAN_NAV_RIGHT = 6,
	TRAN_NAV_CLICK = 7,
	TRAN_NAV_HEAVY = 8,
	TRAN_NAV_LONG_PRESS = 9,
	TRAN_NAV_DOUBLE_CLICK = 10,
};

enum tran_fp_vendor {
	FP_VENDOR_UNKNOWN = 0,
	FP_VENDOR_GOODIX,
	FP_VENDOR_CHIPONE,
};

static unsigned int bufsiz = 4096;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

struct tran_fp_data {
	struct spi_device *spi;
	struct device *dev;
	struct miscdevice miscdev;
	struct input_dev *input;
	struct kobject *kobj;
	struct proc_dir_entry *proc_entry;
	struct wakeup_source *ws;
	struct notifier_block disp_notifier;
	bool disp_notifier_registered;

	/* Locks */
	struct mutex buf_lock;
	struct mutex vendor_lock;
	spinlock_t spi_lock;
	spinlock_t irq_lock;

	/* Pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_irq;
	struct pinctrl_state *pins_irq_pd;
	struct pinctrl_state *pins_reset_high;
	struct pinctrl_state *pins_reset_low;
	struct pinctrl_state *pins_spi_default;
	struct pinctrl_state *pins_spi_gpio;

	/* GPIOs */
	int irq_gpio;
	int reset_gpio;
	int pwr_gpio;

	/* Optional regulators */
	struct regulator *vdd_vmch;
	struct regulator *vdd_vqmmc;
	bool power_on;

	/* SPI clk state */
	bool spi_clk_enabled;
	u8 *spi_buf;

	/* IRQ state */
	int irq;
	bool irq_requested;
	bool irq_enabled;

	/* Chipone poll / fasync state */
	wait_queue_head_t wq_irq_return;
	struct fasync_struct *async_queue;
	int rcv_irq;
	int cancel_wait;
	int fb_status;
	int enable_report_blankon;

	/* Goodix netlink state */
	struct sock *nl_sk;
	u32 netlink_proto;
	int nl_pid;
	u8 fw_info;

	/* Vendor identity */
	enum tran_fp_vendor vendor;
	char vendor_name[64];
	char chip_name[32];
	bool misc_registered;
};

static struct tran_fp_data *g_tran_fp;
static struct platform_device *g_tran_fp_pdev;

/* ---------------- Power & Reset & SPI Clock ---------------- */

static void tran_fp_spi_clk_enable(struct tran_fp_data *fp)
{
	if (!fp || !fp->spi)
		return;
	if (!fp->spi_clk_enabled) {
		mt_spi_enable_master_clk(fp->spi);
		fp->spi_clk_enabled = true;
	}
}

static void tran_fp_spi_clk_disable(struct tran_fp_data *fp)
{
	if (!fp || !fp->spi)
		return;
	if (fp->spi_clk_enabled) {
		mt_spi_disable_master_clk(fp->spi);
		fp->spi_clk_enabled = false;
	}
}

static int tran_fp_power_onoff(struct tran_fp_data *fp, bool on)
{
	int rc = 0;

	if (!fp)
		return -EINVAL;

	if (on == fp->power_on)
		return 0;

	if (on) {
		if (fp->vdd_vmch) {
			rc = regulator_enable(fp->vdd_vmch);
			if (rc) {
				pr_err("vmch regulator_enable failed: %d\n", rc);
				return rc;
			}
		}
		if (fp->vdd_vqmmc) {
			rc = regulator_enable(fp->vdd_vqmmc);
			if (rc) {
				pr_err("vqmmc regulator_enable failed: %d\n", rc);
				if (fp->vdd_vmch)
					regulator_disable(fp->vdd_vmch);
				return rc;
			}
		}
		if (gpio_is_valid(fp->pwr_gpio))
			gpio_direction_output(fp->pwr_gpio, 1);
		fp->power_on = true;
	} else {
		if (gpio_is_valid(fp->pwr_gpio))
			gpio_direction_output(fp->pwr_gpio, 0);
		if (fp->vdd_vqmmc)
			regulator_disable(fp->vdd_vqmmc);
		if (fp->vdd_vmch)
			regulator_disable(fp->vdd_vmch);
		fp->power_on = false;
	}

	return 0;
}

static void tran_fp_set_reset(struct tran_fp_data *fp, bool high)
{
	if (!fp)
		return;

	if (high) {
		if (!IS_ERR_OR_NULL(fp->pins_reset_high))
			pinctrl_select_state(fp->pinctrl, fp->pins_reset_high);
		if (gpio_is_valid(fp->reset_gpio))
			gpio_direction_output(fp->reset_gpio, 1);
	} else {
		if (!IS_ERR_OR_NULL(fp->pins_reset_low))
			pinctrl_select_state(fp->pinctrl, fp->pins_reset_low);
		if (gpio_is_valid(fp->reset_gpio))
			gpio_direction_output(fp->reset_gpio, 0);
	}
}

static void tran_fp_reset_sensor(struct tran_fp_data *fp,
				 unsigned int low_us, unsigned int high_us)
{
	tran_fp_set_reset(fp, false);
	if (low_us)
		usleep_range(low_us, low_us + 500);
	tran_fp_set_reset(fp, true);
	if (high_us)
		usleep_range(high_us, high_us + 500);
}

/* ---------------- IRQ Helpers ---------------- */

static void tran_fp_enable_irq(struct tran_fp_data *fp)
{
	unsigned long flags;

	if (!fp || !fp->irq_requested || fp->irq <= 0)
		return;

	spin_lock_irqsave(&fp->irq_lock, flags);
	if (!fp->irq_enabled) {
		enable_irq(fp->irq);
		fp->irq_enabled = true;
	}
	spin_unlock_irqrestore(&fp->irq_lock, flags);
}

static void tran_fp_disable_irq(struct tran_fp_data *fp)
{
	unsigned long flags;

	if (!fp || !fp->irq_requested || fp->irq <= 0)
		return;

	spin_lock_irqsave(&fp->irq_lock, flags);
	if (fp->irq_enabled) {
		disable_irq_nosync(fp->irq);
		fp->irq_enabled = false;
	}
	spin_unlock_irqrestore(&fp->irq_lock, flags);
}

/* ---------------- Netlink (Goodix) ---------------- */

static void tran_netlink_recv(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	struct tran_fp_data *fp = g_tran_fp;

	if (!fp || !skb || skb->len < nlmsg_total_size(0))
		return;

	nlh = nlmsg_hdr(skb);
	fp->nl_pid = nlh->nlmsg_pid;
	pr_debug("netlink recv pid=%d\n", fp->nl_pid);
}

static int tran_netlink_init(struct tran_fp_data *fp)
{
	struct netlink_kernel_cfg cfg = {
		.input = tran_netlink_recv,
	};

	if (fp->nl_sk)
		return 0;

	fp->nl_sk = netlink_kernel_create(&init_net, fp->netlink_proto, &cfg);
	if (!fp->nl_sk) {
		pr_err("netlink_kernel_create(proto=%u) failed\n",
		       fp->netlink_proto);
		return -ENOMEM;
	}

	pr_info("netlink socket created on proto %u\n", fp->netlink_proto);
	return 0;
}

static void tran_netlink_destroy(struct tran_fp_data *fp)
{
	if (fp && fp->nl_sk) {
		netlink_kernel_release(fp->nl_sk);
		fp->nl_sk = NULL;
	}
}

static void tran_netlink_send(struct tran_fp_data *fp, u8 cmd)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int ret;

	if (!fp || !fp->nl_sk || fp->nl_pid <= 0)
		return;

	skb = nlmsg_new(1, GFP_ATOMIC);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, 1, 0);
	if (!nlh) {
		kfree_skb(skb);
		return;
	}

	*(u8 *)nlmsg_data(nlh) = cmd;
	NETLINK_CB(skb).dst_group = 0;

	ret = netlink_unicast(fp->nl_sk, skb, fp->nl_pid, MSG_DONTWAIT);
	if (ret < 0)
		pr_debug("netlink_unicast(pid=%d, cmd=%u) err=%d\n",
			 fp->nl_pid, cmd, ret);
}

/* ---------------- IRQ Handlers ---------------- */

static irqreturn_t goodix_fp_irq_handler(int irq, void *handle)
{
	struct tran_fp_data *fp = handle;

	if (fp->ws)
		__pm_wakeup_event(fp->ws, 1000);
	tran_netlink_send(fp, GF_NET_EVENT_IRQ);
	return IRQ_HANDLED;
}

static irqreturn_t chipone_fp_irq_handler(int irq, void *handle)
{
	struct tran_fp_data *fp = handle;

	if (fp->ws)
		__pm_wakeup_event(fp->ws, 1000);

	fp->rcv_irq = 1;
	wake_up_interruptible(&fp->wq_irq_return);
	if (fp->async_queue)
		kill_fasync(&fp->async_queue, SIGIO, POLL_IN);

	return IRQ_HANDLED;
}

static int tran_fp_request_irq(struct tran_fp_data *fp, bool is_chipone)
{
	int ret;
	irq_handler_t handler = is_chipone ? chipone_fp_irq_handler
					   : goodix_fp_irq_handler;
	const char *name = is_chipone ? "fpsensor" : "goodix_fp_irq";

	if (fp->irq <= 0) {
		pr_err("no valid IRQ configured\n");
		return -EINVAL;
	}

	if (fp->irq_requested)
		return 0;

	if (!IS_ERR_OR_NULL(fp->pins_irq))
		pinctrl_select_state(fp->pinctrl, fp->pins_irq);

	ret = request_threaded_irq(fp->irq, NULL, handler,
				   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				   name, fp);
	if (ret) {
		pr_err("request_threaded_irq(%d, %s) failed: %d\n",
		       fp->irq, name, ret);
		return ret;
	}

	fp->irq_requested = true;
	fp->irq_enabled = true;
	enable_irq_wake(fp->irq);
	return 0;
}

static void tran_fp_free_irq(struct tran_fp_data *fp)
{
	if (!fp || !fp->irq_requested || fp->irq <= 0)
		return;

	disable_irq_wake(fp->irq);
	free_irq(fp->irq, fp);
	fp->irq_requested = false;
	fp->irq_enabled = false;
}

/* ---------------- Display Blank Notifier ---------------- */

static int tran_fp_disp_notifier_cb(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct tran_fp_data *fp =
		container_of(nb, struct tran_fp_data, disp_notifier);
	int blank;

	if (!data || event != MTK_DISP_EVENT_BLANK)
		return 0;

	blank = *(int *)data;
	if (blank == MTK_DISP_BLANK_UNBLANK) {
		fp->fb_status = 1;
		if (fp->vendor == FP_VENDOR_GOODIX) {
			tran_netlink_send(fp, GF_NET_EVENT_FB_UNBLACK);
		} else if (fp->vendor == FP_VENDOR_CHIPONE &&
			   fp->enable_report_blankon) {
			fp->rcv_irq = 2;
			wake_up_interruptible(&fp->wq_irq_return);
		}
	} else if (blank == MTK_DISP_BLANK_POWERDOWN) {
		fp->fb_status = 0;
		if (fp->vendor == FP_VENDOR_GOODIX)
			tran_netlink_send(fp, GF_NET_EVENT_FB_BLACK);
	}

	return 0;
}

/* ---------------- SPI Chip ID Probing ---------------- */

static int tran_fp_spi_xfer(struct tran_fp_data *fp, const u8 *tx, u8 *rx,
			    size_t len, u32 speed_hz)
{
	struct spi_transfer t = {
		.tx_buf = tx,
		.rx_buf = rx,
		.len = len,
		.bits_per_word = 8,
		.speed_hz = speed_hz,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(fp->spi, &m);
}

static bool tran_probe_chipone(struct tran_fp_data *fp)
{
	u8 *tx, *rx;
	int tries, ret;
	bool found = false;

	tx = kzalloc(16, GFP_KERNEL);
	rx = kzalloc(16, GFP_KERNEL);
	if (!tx || !rx) {
		kfree(tx);
		kfree(rx);
		return false;
	}

	tran_fp_reset_sensor(fp, 2000, 5000);

	for (tries = 4; tries >= 0; tries--) {
		memset(tx, 0, 16);
		memset(rx, 0, 16);
		tx[0] = 0x08;
		tx[1] = 0x55;
		ret = tran_fp_spi_xfer(fp, tx, rx, 6, 1000000);
		if (ret) {
			pr_warn("chipone probe cmd1 spi_sync err=%d\n", ret);
			continue;
		}

		memset(tx, 0, 16);
		memset(rx, 0, 16);
		ret = tran_fp_spi_xfer(fp, tx, rx, 7, 1000000);
		if (ret) {
			pr_warn("chipone probe cmd2 spi_sync err=%d\n", ret);
			continue;
		}

		pr_info("chipone probe chipid: ICNT%02x%02x (try=%d)\n",
			rx[1], rx[2], tries);

		if (rx[1] >= 0x71 && rx[1] <= 0x73) {
			snprintf(fp->chip_name, sizeof(fp->chip_name),
				 "ICNT%02X%02X", rx[1], rx[2]);
			found = true;
			break;
		}
	}

	kfree(tx);
	kfree(rx);
	return found;
}

static bool tran_probe_goodix(struct tran_fp_data *fp)
{
	u8 *tx, *rx;
	int ret;
	bool found = false;

	tx = kzalloc(16, GFP_KERNEL);
	rx = kzalloc(16, GFP_KERNEL);
	if (!tx || !rx) {
		kfree(tx);
		kfree(rx);
		return false;
	}

	tran_fp_reset_sensor(fp, 10000, 10000);

	tx[0] = 0xf0;
	tx[1] = 0x00;
	tx[2] = 0x00;
	ret = tran_fp_spi_xfer(fp, tx, rx, 3, 1000000);
	if (ret) {
		pr_warn("goodix probe cmd1 spi_sync err=%d\n", ret);
		goto out;
	}

	memset(tx, 0, 16);
	memset(rx, 0, 16);
	tx[0] = 0xf1;
	ret = tran_fp_spi_xfer(fp, tx, rx, 5, 1000000);
	if (ret) {
		pr_warn("goodix probe cmd2 spi_sync err=%d\n", ret);
		goto out;
	}

	pr_info("goodix probe chipid: %02x %02x %02x %02x\n",
		rx[1], rx[2], rx[3], rx[4]);

	/* Match Goodix capacitive / optical chip IDs from stock tran_fp */
	if ((rx[1] == 0x12 && (rx[2] == 0x12 || rx[2] == 0x31)) ||
	    rx[4] == 0x26 ||
	    ((rx[1] == 0x05 || rx[1] == 0x06) &&
	     (rx[4] == 0x25 ||
	      (rx[3] == 0x02 && rx[4] == 0x01 && rx[2] >= 0x01 && rx[2] <= 0x03)))) {
		snprintf(fp->chip_name, sizeof(fp->chip_name),
			 "GF_%02X%02X%02X%02X", rx[1], rx[2], rx[3], rx[4]);
		found = true;
	}

out:
	kfree(tx);
	kfree(rx);
	return found;
}

static int tran_get_fp_sensor_chip_id(struct tran_fp_data *fp)
{
	struct device_node *np = fp->dev->of_node;
	bool has_chipone = of_property_read_bool(np, "chipone");
	bool has_goodix = of_property_read_bool(np, "goodix");
	int round;

	/* If neither property is explicitly listed, try both */
	if (!has_chipone && !has_goodix) {
		has_chipone = true;
		has_goodix = true;
	}

	tran_fp_spi_clk_enable(fp);

	for (round = 0; round < 2; round++) {
		if (has_chipone && tran_probe_chipone(fp)) {
			fp->vendor = FP_VENDOR_CHIPONE;
			strscpy(fp->vendor_name, "chipone_tee",
				sizeof(fp->vendor_name));
			fp->miscdev.name = "fpsensor";
			pr_info("detected CHIPONE sensor (%s) -> /dev/fpsensor\n",
				fp->chip_name);
			goto detected;
		}

		if (has_goodix && tran_probe_goodix(fp)) {
			fp->vendor = FP_VENDOR_GOODIX;
			strscpy(fp->vendor_name, "goodix_tee",
				sizeof(fp->vendor_name));
			fp->miscdev.name = "goodix_fp";
			pr_info("detected GOODIX sensor (%s) -> /dev/goodix_fp\n",
				fp->chip_name);
			goto detected;
		}
	}

	tran_fp_spi_clk_disable(fp);
	return -ENODEV;

detected:
	tran_fp_spi_clk_disable(fp);
	return 0;
}

/* ---------------- Input Key / Nav Reporting ---------------- */

static void tran_fp_report_key(struct tran_fp_data *fp, unsigned int code,
			       int value)
{
	if (!fp || !fp->input)
		return;
	input_report_key(fp->input, code, value);
	input_sync(fp->input);
}

static void tran_fp_report_nav(struct tran_fp_data *fp, u32 nav)
{
	unsigned int key = 0;

	switch (nav) {
	case TRAN_NAV_UP:
		key = KEY_UP;
		break;
	case TRAN_NAV_DOWN:
		key = KEY_DOWN;
		break;
	case TRAN_NAV_LEFT:
		key = KEY_LEFT;
		break;
	case TRAN_NAV_RIGHT:
		key = KEY_RIGHT;
		break;
	case TRAN_NAV_CLICK:
		key = KEY_ENTER;
		break;
	case TRAN_NAV_DOUBLE_CLICK:
		key = KEY_F11;
		break;
	case TRAN_NAV_LONG_PRESS:
		key = KEY_HOME;
		break;
	default:
		return;
	}

	tran_fp_report_key(fp, key, 1);
	tran_fp_report_key(fp, key, 0);
}

/* ---------------- File Operations ---------------- */

static int tran_fp_open(struct inode *inode, struct file *filp)
{
	struct tran_fp_data *fp = g_tran_fp;

	if (!fp)
		return -ENODEV;

	filp->private_data = fp;
	return nonseekable_open(inode, filp);
}

static int tran_fp_release(struct inode *inode, struct file *filp)
{
	struct tran_fp_data *fp = filp->private_data;

	if (fp && fp->async_queue)
		fasync_helper(-1, filp, 0, &fp->async_queue);
	filp->private_data = NULL;
	return 0;
}

static __poll_t tran_fp_poll(struct file *filp, poll_table *wait)
{
	struct tran_fp_data *fp = filp->private_data;
	__poll_t mask = 0;

	if (!fp)
		return EPOLLERR;

	poll_wait(filp, &fp->wq_irq_return, wait);

	if (fp->cancel_wait) {
		fp->cancel_wait = 0;
		return EPOLLERR;
	}

	if (fp->rcv_irq == 2) {
		fp->rcv_irq = 0;
		mask |= EPOLLIN | EPOLLPRI;
	} else if (fp->rcv_irq == 1) {
		fp->rcv_irq = 0;
		mask |= EPOLLIN | EPOLLRDNORM;
	}

	return mask;
}

static int tran_fp_fasync(int fd, struct file *filp, int mode)
{
	struct tran_fp_data *fp = filp->private_data;

	if (!fp)
		return -ENODEV;

	return fasync_helper(fd, filp, mode, &fp->async_queue);
}

static long tran_fp_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct tran_fp_data *fp = filp->private_data;
	void __user *uarg = (void __user *)arg;
	long ret = 0;

	if (!fp)
		return -ENODEV;

	mutex_lock(&fp->buf_lock);

	switch (cmd) {
	/* ---- Goodix IOCTLs ---- */
	case GF_IOC_INIT: {
		u8 netlink_route = (u8)fp->netlink_proto;

		if (copy_to_user(uarg, &netlink_route, sizeof(netlink_route))) {
			ret = -EFAULT;
			break;
		}
		ret = tran_fp_request_irq(fp, false);
		break;
	}
	case GF_IOC_EXIT:
		tran_fp_free_irq(fp);
		break;
	case GF_IOC_RESET:
		tran_fp_reset_sensor(fp, 3000, 3000);
		break;
	case GF_IOC_ENABLE_IRQ:
		tran_fp_enable_irq(fp);
		break;
	case GF_IOC_DISABLE_IRQ:
		tran_fp_disable_irq(fp);
		break;
	case GF_IOC_ENABLE_SPI_CLK:
		tran_fp_spi_clk_enable(fp);
		break;
	case GF_IOC_DISABLE_SPI_CLK:
		tran_fp_spi_clk_disable(fp);
		break;
	case GF_IOC_ENABLE_POWER:
		ret = tran_fp_power_onoff(fp, true);
		break;
	case GF_IOC_DISABLE_POWER:
		ret = tran_fp_power_onoff(fp, false);
		break;
	case GF_IOC_INPUT_KEY_EVENT: {
		struct gf_key gk;

		if (copy_from_user(&gk, uarg, sizeof(gk))) {
			ret = -EFAULT;
			break;
		}
		tran_fp_report_key(fp, gk.key, gk.value);
		break;
	}
	case GF_IOC_ENTER_SLEEP_MODE:
		break;
	case GF_IOC_GET_FW_INFO:
		if (copy_to_user(uarg, &fp->fw_info, sizeof(fp->fw_info)))
			ret = -EFAULT;
		break;
	case GF_IOC_REMOVE:
		tran_fp_free_irq(fp);
		break;
	case GF_IOC_CHIP_INFO: {
		struct gf_ioc_chip_info info;

		if (copy_from_user(&info, uarg, sizeof(info))) {
			ret = -EFAULT;
			break;
		}
		pr_info("gf chip info: vendor=0x%02x mode=0x%02x op=0x%02x\n",
			info.vendor_id, info.mode, info.operation);
		break;
	}
	case GF_IOC_NAV_EVENT: {
		u32 nav = 0;

		if (copy_from_user(&nav, uarg, sizeof(nav))) {
			ret = -EFAULT;
			break;
		}
		tran_fp_report_nav(fp, nav);
		break;
	}

	/* ---- Chipone IOCTLs ---- */
	case FPSENSOR_IOC_INIT:
		fp->cancel_wait = 0;
		fp->rcv_irq = 0;
		ret = tran_fp_request_irq(fp, true);
		break;
	case FPSENSOR_IOC_EXIT:
		tran_fp_free_irq(fp);
		break;
	case FPSENSOR_IOC_RESET: {
		u32 delay_ms = 3;

		if (uarg && !copy_from_user(&delay_ms, uarg, sizeof(delay_ms))) {
			if (delay_ms > 100)
				delay_ms = 100;
		}
		tran_fp_reset_sensor(fp, delay_ms * 1000, delay_ms * 1000);
		break;
	}
	case FPSENSOR_IOC_ENABLE_IRQ:
		fp->cancel_wait = 0;
		tran_fp_enable_irq(fp);
		break;
	case FPSENSOR_IOC_DISABLE_IRQ:
		tran_fp_disable_irq(fp);
		break;
	case FPSENSOR_IOC_GET_INT_VAL: {
		u32 val = gpio_is_valid(fp->irq_gpio)
				  ? (u32)gpio_get_value(fp->irq_gpio)
				  : 0;
		if (copy_to_user(uarg, &val, sizeof(val)))
			ret = -EFAULT;
		break;
	}
	case FPSENSOR_IOC_DISABLE_SPI_CLK:
		tran_fp_spi_clk_disable(fp);
		break;
	case FPSENSOR_IOC_ENABLE_SPI_CLK:
		tran_fp_spi_clk_enable(fp);
		break;
	case FPSENSOR_IOC_ENABLE_POWER:
		ret = tran_fp_power_onoff(fp, true);
		break;
	case FPSENSOR_IOC_DISABLE_POWER:
		ret = tran_fp_power_onoff(fp, false);
		break;
	case FPSENSOR_IOC_REMOVE:
		tran_fp_free_irq(fp);
		break;
	case FPSENSOR_IOC_CANCEL_WAIT:
		fp->cancel_wait = 1;
		wake_up_interruptible(&fp->wq_irq_return);
		break;
	case FPSENSOR_IOC_GET_FP_STATUS: {
		u32 status = (u32)fp->fb_status;

		if (copy_to_user(uarg, &status, sizeof(status)))
			ret = -EFAULT;
		break;
	}
	case FPSENSOR_IOC_ENABLE_REPORT_BLANKON: {
		u32 en = 0;

		if (copy_from_user(&en, uarg, sizeof(en))) {
			ret = -EFAULT;
			break;
		}
		fp->enable_report_blankon = en ? 1 : 0;
		break;
	}
	case FPSENSOR_IOC_UPDATE_DRIVER_SN:
		break;

	default:
		pr_warn("unsupported ioctl cmd=0x%08x (type=0x%02x nr=%u)\n",
			cmd, _IOC_TYPE(cmd), _IOC_NR(cmd));
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&fp->buf_lock);
	return ret;
}

#ifdef CONFIG_COMPAT
static long tran_fp_compat_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	return tran_fp_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations tran_fp_fops = {
	.owner		= THIS_MODULE,
	.open		= tran_fp_open,
	.release	= tran_fp_release,
	.unlocked_ioctl	= tran_fp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= tran_fp_compat_ioctl,
#endif
	.poll		= tran_fp_poll,
	.fasync		= tran_fp_fasync,
};

/* ---------------- Sysfs (/sys/kernel/tran_fp/vendor_name) ---------------- */

static ssize_t vendor_name_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct tran_fp_data *fp = g_tran_fp;
	ssize_t len;

	if (!fp)
		return sysfs_emit(buf, "unknown\n");

	mutex_lock(&fp->vendor_lock);
	len = sysfs_emit(buf, "%s\n", fp->vendor_name);
	mutex_unlock(&fp->vendor_lock);
	return len;
}

static ssize_t vendor_name_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	struct tran_fp_data *fp = g_tran_fp;
	size_t n;

	if (!fp)
		return -ENODEV;

	mutex_lock(&fp->vendor_lock);
	n = min(count, sizeof(fp->vendor_name) - 1);
	memcpy(fp->vendor_name, buf, n);
	fp->vendor_name[n] = '\0';
	strim(fp->vendor_name);
	mutex_unlock(&fp->vendor_lock);
	return count;
}

static struct kobj_attribute vendor_name_attr =
	__ATTR(vendor_name, 0664, vendor_name_show, vendor_name_store);

static struct attribute *tran_fp_attrs[] = {
	&vendor_name_attr.attr,
	NULL,
};

static const struct attribute_group tran_fp_attr_group = {
	.attrs = tran_fp_attrs,
};

/* ---------------- Procfs (/proc/fpsensor for Chipone) ---------------- */

static int fpsensor_proc_show(struct seq_file *m, void *v)
{
	struct tran_fp_data *fp = g_tran_fp;

	seq_printf(m, "%s\n", fp ? fp->chip_name : "unknown");
	return 0;
}

static int fpsensor_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fpsensor_proc_show, NULL);
}

static const struct proc_ops fpsensor_proc_ops = {
	.proc_open	= fpsensor_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

/* ---------------- DTS & Input Init ---------------- */

static int tran_fp_parse_dts(struct tran_fp_data *fp)
{
	struct device *dev = fp->dev;
	struct device_node *np = dev->of_node;
	u32 nl_proto = TRAN_FP_NETLINK_DEFAULT;
	int ret;

	if (!np) {
		pr_err("missing OF device node\n");
		return -ENODEV;
	}

	of_property_read_u32(np, "netlink-event", &nl_proto);
	fp->netlink_proto = nl_proto;

	/* Optional pinctrl */
	fp->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fp->pinctrl)) {
		pr_warn("devm_pinctrl_get failed (%ld), continuing without pinctrl\n",
			PTR_ERR(fp->pinctrl));
		fp->pinctrl = NULL;
	} else {
		fp->pins_default = pinctrl_lookup_state(fp->pinctrl, "tran_fp_default");
		fp->pins_irq = pinctrl_lookup_state(fp->pinctrl, "tran_fp_pin_irq");
		fp->pins_irq_pd = pinctrl_lookup_state(fp->pinctrl, "tran_fp_irq_pull_down");
		fp->pins_reset_high = pinctrl_lookup_state(fp->pinctrl, "tran_fp_reset_high");
		fp->pins_reset_low = pinctrl_lookup_state(fp->pinctrl, "tran_fp_reset_low");
		fp->pins_spi_default = pinctrl_lookup_state(fp->pinctrl, "tran_fp_spi_default");
		fp->pins_spi_gpio = pinctrl_lookup_state(fp->pinctrl, "tran_fp_spi_gpio");

		if (!IS_ERR_OR_NULL(fp->pins_spi_default)) {
			ret = pinctrl_select_state(fp->pinctrl, fp->pins_spi_default);
			if (ret)
				pr_warn("select tran_fp_spi_default failed: %d\n", ret);
		}
	}

	/* GPIOs (prefer *-std phandle properties) */
	fp->reset_gpio = of_get_named_gpio(np, "reset-gpio-std", 0);
	if (!gpio_is_valid(fp->reset_gpio))
		fp->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (gpio_is_valid(fp->reset_gpio)) {
		ret = devm_gpio_request(dev, fp->reset_gpio, "tran_fp_reset");
		if (ret) {
			pr_warn("request reset gpio %d failed: %d\n",
				fp->reset_gpio, ret);
			fp->reset_gpio = -EINVAL;
		}
	}

	fp->irq_gpio = of_get_named_gpio(np, "irq-gpio-std", 0);
	if (!gpio_is_valid(fp->irq_gpio))
		fp->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (gpio_is_valid(fp->irq_gpio)) {
		ret = devm_gpio_request(dev, fp->irq_gpio, "tran_fp_irq");
		if (ret) {
			pr_warn("request irq gpio %d failed: %d\n",
				fp->irq_gpio, ret);
			fp->irq_gpio = -EINVAL;
		} else {
			gpio_direction_input(fp->irq_gpio);
		}
	}

	fp->pwr_gpio = of_get_named_gpio(np, "tran,gpio_pwr-std", 0);
	if (!gpio_is_valid(fp->pwr_gpio))
		fp->pwr_gpio = of_get_named_gpio(np, "tran,gpio_pwr", 0);
	if (gpio_is_valid(fp->pwr_gpio)) {
		ret = devm_gpio_request(dev, fp->pwr_gpio, "tran_fp_pwr");
		if (ret) {
			pr_warn("request pwr gpio %d failed: %d\n",
				fp->pwr_gpio, ret);
			fp->pwr_gpio = -EINVAL;
		}
	}

	/* Optional regulators */
	if (of_find_property(np, "vmch-supply", NULL)) {
		fp->vdd_vmch = devm_regulator_get_optional(dev, "vmch");
		if (IS_ERR(fp->vdd_vmch))
			fp->vdd_vmch = NULL;
	}
	if (of_find_property(np, "vqmmc-supply", NULL)) {
		fp->vdd_vqmmc = devm_regulator_get_optional(dev, "vqmmc");
		if (IS_ERR(fp->vdd_vqmmc))
			fp->vdd_vqmmc = NULL;
	}

	/* IRQ resolution */
	fp->irq = fp->spi->irq;
	if (fp->irq <= 0)
		fp->irq = irq_of_parse_and_map(np, 0);
	if (fp->irq <= 0 && gpio_is_valid(fp->irq_gpio))
		fp->irq = gpio_to_irq(fp->irq_gpio);

	pr_info("parsed DTS: irq=%d irq_gpio=%d rst_gpio=%d pwr_gpio=%d nl=%u\n",
		fp->irq, fp->irq_gpio, fp->reset_gpio, fp->pwr_gpio,
		fp->netlink_proto);
	return 0;
}

static int tran_fp_input_init(struct tran_fp_data *fp)
{
	int ret;

	fp->input = devm_input_allocate_device(fp->dev);
	if (!fp->input)
		return -ENOMEM;

	fp->input->name = "tran_fp_input";
	input_set_capability(fp->input, EV_KEY, KEY_HOME);
	input_set_capability(fp->input, EV_KEY, KEY_BACK);
	input_set_capability(fp->input, EV_KEY, KEY_POWER);
	input_set_capability(fp->input, EV_KEY, KEY_WAKEUP);
	input_set_capability(fp->input, EV_KEY, KEY_UP);
	input_set_capability(fp->input, EV_KEY, KEY_DOWN);
	input_set_capability(fp->input, EV_KEY, KEY_LEFT);
	input_set_capability(fp->input, EV_KEY, KEY_RIGHT);
	input_set_capability(fp->input, EV_KEY, KEY_ENTER);
	input_set_capability(fp->input, EV_KEY, KEY_F11);

	ret = input_register_device(fp->input);
	if (ret) {
		pr_err("input_register_device failed: %d\n", ret);
		fp->input = NULL;
		return ret;
	}

	return 0;
}

/* ---------------- SPI Driver Probe / Remove ---------------- */

static int tran_fp_spi_probe(struct spi_device *spi)
{
	struct tran_fp_data *fp;
	int ret;

	pr_info("tran_fp_spi_probe enter\n");

	fp = devm_kzalloc(&spi->dev, sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return -ENOMEM;

	fp->spi = spi;
	fp->dev = &spi->dev;
	fp->irq_gpio = -EINVAL;
	fp->reset_gpio = -EINVAL;
	fp->pwr_gpio = -EINVAL;
	fp->fb_status = 1;

	mutex_init(&fp->buf_lock);
	mutex_init(&fp->vendor_lock);
	spin_lock_init(&fp->spi_lock);
	spin_lock_init(&fp->irq_lock);
	init_waitqueue_head(&fp->wq_irq_return);

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = 4000000;
	ret = spi_setup(spi);
	if (ret)
		pr_warn("spi_setup returned %d\n", ret);

	fp->spi_buf = devm_kzalloc(&spi->dev, bufsiz, GFP_KERNEL);
	if (!fp->spi_buf)
		return -ENOMEM;

	spi_set_drvdata(spi, fp);
	g_tran_fp = fp;

	ret = tran_fp_parse_dts(fp);
	if (ret)
		goto err_clear_global;

	fp->ws = wakeup_source_register(&spi->dev, "tran_fp_ws");

	ret = tran_fp_power_onoff(fp, true);
	if (ret)
		goto err_ws;

	usleep_range(10000, 12000);

	ret = tran_get_fp_sensor_chip_id(fp);
	if (ret) {
		pr_err("no supported fingerprint sensor detected (err=%d)\n", ret);
		goto err_power;
	}

	/* Create /sys/kernel/tran_fp/vendor_name */
	fp->kobj = kobject_create_and_add("tran_fp", kernel_kobj);
	if (fp->kobj) {
		ret = sysfs_create_group(fp->kobj, &tran_fp_attr_group);
		if (ret)
			pr_warn("sysfs_create_group failed: %d\n", ret);
	} else {
		pr_warn("failed to create /sys/kernel/tran_fp\n");
	}

	/* Register /dev/fpsensor or /dev/goodix_fp */
	fp->miscdev.minor = MISC_DYNAMIC_MINOR;
	fp->miscdev.fops = &tran_fp_fops;
	fp->miscdev.parent = &spi->dev;
	ret = misc_register(&fp->miscdev);
	if (ret) {
		pr_err("misc_register(%s) failed: %d\n",
		       fp->miscdev.name, ret);
		goto err_sysfs;
	}
	fp->misc_registered = true;

	/* Chipone procfs node */
	if (fp->vendor == FP_VENDOR_CHIPONE) {
		fp->proc_entry = proc_create("fpsensor", 0444, NULL,
					     &fpsensor_proc_ops);
		if (!fp->proc_entry)
			pr_warn("proc_create(/proc/fpsensor) failed\n");
	}

	/* Input device (non-fatal if it fails) */
	tran_fp_input_init(fp);

	/* Goodix netlink socket */
	if (fp->vendor == FP_VENDOR_GOODIX) {
		ret = tran_netlink_init(fp);
		if (ret)
			goto err_misc;
	}

	/* MTK display blank notifier */
	fp->disp_notifier.notifier_call = tran_fp_disp_notifier_cb;
	ret = mtk_disp_notifier_register("FINGERPRINT", &fp->disp_notifier);
	if (ret)
		pr_warn("mtk_disp_notifier_register failed: %d\n", ret);
	else
		fp->disp_notifier_registered = true;

	pr_info("tran_fp_spi_probe success: vendor=%s dev=/dev/%s\n",
		fp->vendor_name, fp->miscdev.name);
	return 0;

err_misc:
	if (fp->proc_entry)
		proc_remove(fp->proc_entry);
	if (fp->misc_registered)
		misc_deregister(&fp->miscdev);
err_sysfs:
	if (fp->kobj) {
		sysfs_remove_group(fp->kobj, &tran_fp_attr_group);
		kobject_put(fp->kobj);
	}
err_power:
	tran_fp_power_onoff(fp, false);
err_ws:
	if (fp->ws)
		wakeup_source_unregister(fp->ws);
err_clear_global:
	g_tran_fp = NULL;
	return ret;
}

static void tran_fp_spi_remove(struct spi_device *spi)
{
	struct tran_fp_data *fp = spi_get_drvdata(spi);

	if (!fp)
		return;

	if (fp->disp_notifier_registered)
		mtk_disp_notifier_unregister(&fp->disp_notifier);

	tran_fp_free_irq(fp);
	tran_netlink_destroy(fp);

	if (fp->proc_entry)
		proc_remove(fp->proc_entry);

	if (fp->misc_registered)
		misc_deregister(&fp->miscdev);

	if (fp->kobj) {
		sysfs_remove_group(fp->kobj, &tran_fp_attr_group);
		kobject_put(fp->kobj);
	}

	tran_fp_spi_clk_disable(fp);
	tran_fp_power_onoff(fp, false);

	if (fp->ws)
		wakeup_source_unregister(fp->ws);

	g_tran_fp = NULL;
	pr_info("tran_fp_spi_remove done\n");
}

static const struct of_device_id tran_fp_of_match[] = {
	{ .compatible = "tran_fp" },
	{ }
};
MODULE_DEVICE_TABLE(of, tran_fp_of_match);

static const struct spi_device_id tran_fp_spi_id[] = {
	{ "tran_fp", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, tran_fp_spi_id);

static struct spi_driver tran_fp_spi_driver = {
	.driver = {
		.name		= "tran_fp",
		.of_match_table	= tran_fp_of_match,
	},
	.id_table	= tran_fp_spi_id,
	.probe		= tran_fp_spi_probe,
	.remove		= tran_fp_spi_remove,
};

/* ---------------- Platform Driver (matches odm/fingerprint) ---------------- */

static int tran_fp_platform_probe(struct platform_device *pdev)
{
	g_tran_fp_pdev = pdev;
	pr_info("tran_fp_platform_probe matched %s\n", dev_name(&pdev->dev));
	return 0;
}

static void tran_fp_platform_remove(struct platform_device *pdev)
{
	if (g_tran_fp_pdev == pdev)
		g_tran_fp_pdev = NULL;
}

static const struct of_device_id tran_fp_plat_of_match[] = {
	{ .compatible = "tran_fp" },
	{ }
};

static struct platform_driver tran_fp_platform_driver = {
	.driver = {
		.name		= "tran_fp_plat",
		.of_match_table	= tran_fp_plat_of_match,
	},
	.probe	= tran_fp_platform_probe,
	.remove	= tran_fp_platform_remove,
};

/* ---------------- Module Init / Exit ---------------- */

static int __init tran_fp_init(void)
{
	int ret;

	ret = platform_driver_register(&tran_fp_platform_driver);
	if (ret)
		pr_warn("platform_driver_register failed: %d\n", ret);

	ret = spi_register_driver(&tran_fp_spi_driver);
	if (ret) {
		pr_err("spi_register_driver failed: %d\n", ret);
		platform_driver_unregister(&tran_fp_platform_driver);
		return ret;
	}

	return 0;
}

static void __exit tran_fp_exit(void)
{
	spi_unregister_driver(&tran_fp_spi_driver);
	platform_driver_unregister(&tran_fp_platform_driver);
}

module_init(tran_fp_init);
module_exit(tran_fp_exit);

MODULE_AUTHOR("Transsion Holdings");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
