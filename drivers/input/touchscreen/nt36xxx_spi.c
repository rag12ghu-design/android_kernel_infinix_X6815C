// SPDX-License-Identifier: GPL-2.0
/*
 * Novatek NT36xxx SPI Touchscreen Driver for Infinix Zero 5G 2023 (X6815C / MT6877)
 *
 * Implements Multi-Touch Protocol Type B for Novatek NT36xxx on SPI4.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/bitmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <asm/unaligned.h>

#define NVT_TS_NAME             "mtk-tpd"
#define NVT_DRIVER_NAME         "novatek-nvt-ts-spi"
#define NVT_MAX_TOUCH_POINTS    10
#define NVT_TOUCH_PACKET_LEN    66
#define NVT_POINT_DATA_LEN      6

#define NVT_DEFAULT_X_MAX       1079
#define NVT_DEFAULT_Y_MAX       2459

#define NVT_DEF_SWRST_ADDR      0x03F0FE
#define NVT_DEF_FAST_RD_ADDR    0x03F310

#define NVT_CHUNK_SIZE          4096

/* Default MT6877 PIO lines for Infinix Zero 5G 2023 */
#define NVT_DEFAULT_RESET_GPIO  15
#define NVT_DEFAULT_IRQ_GPIO    14

struct nvt_ts_data {
	struct spi_device *spi;
	struct input_dev *input_dev;
	struct regulator *vtouch;
	int reset_gpio;
	int irq_gpio;
	int irq;
	u32 swrst_n8_addr;
	u32 spi_rd_fast_addr;
	u32 max_x;
	u32 max_y;
	u8 *tx_buf;
	u8 *rx_buf;
	struct mutex lock;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_spi_mode;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_rst_low;
	struct pinctrl_state *pin_rst_high;
	bool fw_loaded;
};

/*
 * Stock CTP_SPI_READ (0xffffff972828e7d4):
 * Single full-duplex transfer of (len + 1) bytes.
 * tx_buf[0] = buf[0] & 0x7F, tx_buf[1..len] = 0x00.
 * rx_buf receives (len + 1) bytes.
 * On return, copies rx_buf + 2 to buf + 1 (len - 1 bytes).
 */
static int ctp_spi_read(struct nvt_ts_data *ts, u8 *buf, size_t len)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (!buf || len == 0 || len >= 250)
		return -EINVAL;

	memset(ts->tx_buf, 0, len + 1);
	memset(ts->rx_buf, 0, len + 1);

	buf[0] &= 0x7F;
	memcpy(ts->tx_buf, buf, len);
	ts->tx_buf[len] = 0x00;

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = ts->tx_buf;
	xfer.rx_buf = ts->rx_buf;
	xfer.len = len + 1;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(ts->spi, &msg);
	if (ret < 0) {
		dev_err_ratelimited(&ts->spi->dev, "CTP_SPI_READ failed: %d\n", ret);
		return ret;
	}

	memcpy(buf + 1, ts->rx_buf + 2, len - 1);
	return 0;
}

static irqreturn_t nvt_ts_irq_thread(int irq, void *dev_id)
{
	struct nvt_ts_data *ts = dev_id;
	u8 buf[68];
	int ret, i;
	u32 sum = 0;
	bool any_touch = false;
	DECLARE_BITMAP(active_slots, NVT_MAX_TOUCH_POINTS);

	bitmap_zero(active_slots, NVT_MAX_TOUCH_POINTS);

	mutex_lock(&ts->lock);

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x00;

	/* Stock reads exactly 66 bytes via CTP_SPI_READ (len=66 -> transfer=67) */
	ret = ctp_spi_read(ts, buf, NVT_TOUCH_PACKET_LEN);
	if (ret < 0) {
		dev_err_ratelimited(&ts->spi->dev, "SPI touch packet read failed: %d\n", ret);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}

	/* Stock checksum verification: ((-sum(buf[1..64])) & 0xff) == buf[65] */
	for (i = 1; i <= 64; i++)
		sum += buf[i];

	if (((-sum) & 0xff) != buf[65]) {
		dev_warn_ratelimited(&ts->spi->dev,
			"checksum error: sum=0x%02x, csum=0x%02x, expected=0x%02x\n",
			sum & 0xff, (-sum) & 0xff, buf[65]);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}

	/* Stock gesture key handling */
	if (buf[1] == 0xF0 && buf[2] == 0x04) {
		if (buf[3] == 0x02) {
			input_report_key(ts->input_dev, KEY_F14, 1);
			input_sync(ts->input_dev);
			input_report_key(ts->input_dev, KEY_F14, 0);
			input_sync(ts->input_dev);
			mutex_unlock(&ts->lock);
			return IRQ_HANDLED;
		} else if (buf[3] == 0x01) {
			input_report_key(ts->input_dev, KEY_F13, 1);
			input_sync(ts->input_dev);
			input_report_key(ts->input_dev, KEY_F13, 0);
			input_sync(ts->input_dev);
			mutex_unlock(&ts->lock);
			return IRQ_HANDLED;
		}
	}

	for (i = 0; i < NVT_MAX_TOUCH_POINTS; i++) {
		int ppos = 1 + i * NVT_POINT_DATA_LEN;
		u8 id_byte = buf[ppos];
		int input_id = id_byte >> 3;
		u8 type = id_byte & 0x07;

		if (input_id < 1 || input_id > NVT_MAX_TOUCH_POINTS)
			continue;

		/* type 1 = DOWN, type 2 = CONTACT; ignore other states */
		if (type != 1 && type != 2)
			continue;

		{
			int slot = input_id - 1;
			int x = (buf[ppos + 1] << 4) | (buf[ppos + 3] >> 4);
			int y = (buf[ppos + 2] << 4) | (buf[ppos + 3] & 0x0f);
			int w = buf[ppos + 4];
			int p = buf[ppos + 5];

			if (x < 0) x = 0;
			if (x > ts->max_x) x = ts->max_x;
			if (y < 0) y = 0;
			if (y > ts->max_y) y = ts->max_y;
			if (w <= 0) w = 1;
			if (w > 100) w = 100;
			if (p <= 0) p = 1;
			if (p > 1000) p = 1000;

			input_mt_slot(ts->input_dev, slot);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, p);

			if (!any_touch) {
				input_report_abs(ts->input_dev, ABS_X, x);
				input_report_abs(ts->input_dev, ABS_Y, y);
				input_report_abs(ts->input_dev, ABS_PRESSURE, p > 255 ? 255 : p);
			}
			__set_bit(slot, active_slots);
			any_touch = true;
		}
	}

	/* Synchronize lifted slots */
	for (i = 0; i < NVT_MAX_TOUCH_POINTS; i++) {
		if (!test_bit(i, active_slots)) {
			input_mt_slot(ts->input_dev, i);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		}
	}

	input_report_key(ts->input_dev, BTN_TOUCH, any_touch);
	if (!any_touch)
		input_report_abs(ts->input_dev, ABS_PRESSURE, 0);

	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);

	mutex_unlock(&ts->lock);
	return IRQ_HANDLED;
}

static int nvt_check_chip_id(struct nvt_ts_data *ts)
{
	u8 addr_cmd[3];
	u8 buf[8];
	int ret;

	/* 1. Read chip ver trim at 0x01F64E */
	addr_cmd[0] = 0xFF;
	addr_cmd[1] = (0x01F64E >> 15) & 0xFF;
	addr_cmd[2] = (0x01F64E >> 7) & 0xFF;
	ret = spi_write(ts->spi, addr_cmd, sizeof(addr_cmd));
	if (ret >= 0) {
		memset(buf, 0, sizeof(buf));
		buf[0] = 0x01F64E & 0x7F;
		ret = ctp_spi_read(ts, buf, 7);
		if (ret == 0 && buf[1] != 0x00 && buf[1] != 0xFF) {
			dev_info(&ts->spi->dev, "chip trim at 0x01F64E: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
				 buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
			return 0;
		}
	}

	/* 2. Alternative NT36528 / NT36672 check at 0x03F004 */
	addr_cmd[0] = 0xFF;
	addr_cmd[1] = (0x03F004 >> 15) & 0xFF;
	addr_cmd[2] = (0x03F004 >> 7) & 0xFF;
	ret = spi_write(ts->spi, addr_cmd, sizeof(addr_cmd));
	if (ret >= 0) {
		memset(buf, 0, sizeof(buf));
		buf[0] = 0x03F004 & 0x7F;
		ret = ctp_spi_read(ts, buf, 7);
		if (ret == 0 && buf[1] != 0x00 && buf[1] != 0xFF) {
			dev_info(&ts->spi->dev, "chip trim at 0x03F004: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
				 buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
			return 0;
		}
	}

	dev_warn(&ts->spi->dev, "chip trim check failed: 0x01F64E and 0x03F004 both invalid\n");
	return -ENODEV;
}

static int nvt_write_sram(struct nvt_ts_data *ts, const u8 *data, size_t file_offset, u32 sram_addr, size_t length)
{
	size_t remaining = length;
	u8 *xfer_buf;
	u8 addr_cmd[3];
	int ret = 0;

	xfer_buf = kmalloc(NVT_CHUNK_SIZE + 1, GFP_KERNEL);
	if (!xfer_buf)
		return -ENOMEM;

	while (remaining > 0) {
		size_t chunk_len = min_t(size_t, remaining, NVT_CHUNK_SIZE);

		/* 1. Target SRAM address selection */
		addr_cmd[0] = 0xFF;
		addr_cmd[1] = (sram_addr >> 15) & 0xFF;
		addr_cmd[2] = (sram_addr >> 7) & 0xFF;
		ret = spi_write(ts->spi, addr_cmd, sizeof(addr_cmd));
		if (ret < 0) {
			dev_err(&ts->spi->dev, "SPI addr_cmd failed at sram_addr 0x%06x: %d\n", sram_addr, ret);
			break;
		}

		/* 2. Stream chunk payload */
		xfer_buf[0] = sram_addr & 0x7F;
		memcpy(&xfer_buf[1], data + file_offset, chunk_len);
		ret = spi_write(ts->spi, xfer_buf, chunk_len + 1);
		if (ret < 0) {
			dev_err(&ts->spi->dev, "SPI stream payload failed at sram_addr 0x%06x, len %zu: %d\n",
				sram_addr, chunk_len, ret);
			break;
		}

		sram_addr += chunk_len;
		file_offset += chunk_len;
		remaining -= chunk_len;
	}

	kfree(xfer_buf);
	return ret;
}

static int nvt_boot_ic(struct nvt_ts_data *ts)
{
	u8 addr_cmd[3];
	u8 tx[2];
	u8 rx[2];
	u8 write_cmd[2];
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	/* 1. Target SRAM address selection: swrst_n8_addr */
	addr_cmd[0] = 0xFF;
	addr_cmd[1] = (ts->swrst_n8_addr >> 15) & 0xFF;
	addr_cmd[2] = (ts->swrst_n8_addr >> 7) & 0xFF;
	ret = spi_write(ts->spi, addr_cmd, sizeof(addr_cmd));
	if (ret < 0) {
		dev_err(&ts->spi->dev, "SPI addr_cmd failed for swrst 0x%06x: %d\n", ts->swrst_n8_addr, ret);
		return ret;
	}

	/* 2. Read register at swrst_n8_addr via 2-byte full-duplex transfer (matches stock 0xc0ef40) */
	memset(tx, 0, sizeof(tx));
	memset(rx, 0, sizeof(rx));
	tx[0] = ts->swrst_n8_addr & 0x7F;
	tx[1] = 0x00;

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = tx;
	xfer.rx_buf = rx;
	xfer.len = 2;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(ts->spi, &msg);
	if (ret < 0) {
		dev_err(&ts->spi->dev, "SPI read failed for swrst reg 0x%06x: %d\n", ts->swrst_n8_addr, ret);
		return ret;
	}

	/* 3. Trigger boot: preserve config bits, set bit 7 (val | 0x80) and write back */
	write_cmd[0] = ts->swrst_n8_addr & 0x7F;
	write_cmd[1] = rx[1] | 0x80;
	ret = spi_write(ts->spi, write_cmd, sizeof(write_cmd));
	if (ret < 0) {
		dev_err(&ts->spi->dev, "SPI write failed for swrst reg 0x%06x: %d\n", ts->swrst_n8_addr, ret);
		return ret;
	}

	/* 4. Wait for boot */
	msleep(160);
	dev_info(&ts->spi->dev, "Touch IC boot triggered (reg 0x%06x: 0x%02x -> 0x%02x), msleep(160) completed\n",
		 ts->swrst_n8_addr, rx[1], write_cmd[1]);
	return 0;
}

static int nvt_load_firmware(struct nvt_ts_data *ts, const struct firmware *fw)
{
	u32 ilm_off, ilm_addr, ilm_len;
	u32 dlm_off, dlm_addr, dlm_len;
	int ret;

	if (!fw || fw->size < 0x18100 + 17791) {
		dev_err(&ts->spi->dev, "firmware image invalid or truncated (size %zu)\n", fw ? fw->size : 0);
		return -EINVAL;
	}

	ilm_off = get_unaligned_le32(fw->data + 0);
	ilm_addr = get_unaligned_le32(fw->data + 4);
	ilm_len = get_unaligned_le32(fw->data + 8);
	dlm_off = get_unaligned_le32(fw->data + 12);
	dlm_addr = get_unaligned_le32(fw->data + 16);
	dlm_len = get_unaligned_le32(fw->data + 20);

	/* Sanity check header; fallback to stock layout if invalid */
	if (ilm_off != 0x100 || ilm_len == 0 || ilm_off + ilm_len > fw->size ||
	    dlm_off != 0x18100 || dlm_len == 0 || dlm_off + dlm_len > fw->size) {
		ilm_off = 0x100;
		ilm_addr = 0x00000000;
		ilm_len = 73095;
		dlm_off = 0x18100;
		dlm_addr = 0x00027140;
		dlm_len = 17791;
	}

	dev_info(&ts->spi->dev, "Streaming ILM (offset 0x%x, SRAM 0x%06x, len %u)...\n",
		 ilm_off, ilm_addr, ilm_len);
	ret = nvt_write_sram(ts, fw->data, ilm_off, ilm_addr, ilm_len);
	if (ret) {
		dev_err(&ts->spi->dev, "Failed to stream ILM: %d\n", ret);
		return ret;
	}

	dev_info(&ts->spi->dev, "Streaming DLM (offset 0x%x, SRAM 0x%06x, len %u)...\n",
		 dlm_off, dlm_addr, dlm_len);
	ret = nvt_write_sram(ts, fw->data, dlm_off, dlm_addr, dlm_len);
	if (ret) {
		dev_err(&ts->spi->dev, "Failed to stream DLM: %d\n", ret);
		return ret;
	}

	/* Trigger boot and wait */
	ret = nvt_boot_ic(ts);
	if (ret) {
		dev_err(&ts->spi->dev, "Failed to boot touch IC: %d\n", ret);
		return ret;
	}

	ts->fw_loaded = true;
	dev_info(&ts->spi->dev, "Novatek touchscreen firmware streamed and IC booted successfully\n");
	return 0;
}

static void nvt_ts_mp_callback(const struct firmware *fw, void *context)
{
	struct nvt_ts_data *ts = context;
	int ret;

	if (!fw) {
		dev_warn(&ts->spi->dev, "no novatek_ts_mp.bin found\n");
		return;
	}

	dev_info(&ts->spi->dev, "found firmware novatek_ts_mp.bin (%zu bytes), streaming to SRAM...\n", fw->size);
	mutex_lock(&ts->lock);
	ret = nvt_load_firmware(ts, fw);
	mutex_unlock(&ts->lock);
	if (ret)
		dev_err(&ts->spi->dev, "novatek_ts_mp.bin firmware load failed: %d\n", ret);
	else
		dev_info(&ts->spi->dev, "novatek_ts_mp.bin loaded and IC booted successfully\n");

	release_firmware(fw);
}

static void nvt_ts_fw_callback(const struct firmware *fw, void *context)
{
	struct nvt_ts_data *ts = context;
	int ret;

	if (!fw) {
		dev_info(&ts->spi->dev, "novatek_ts_fw.bin not found; trying novatek_ts_mp.bin\n");
		request_firmware_nowait(THIS_MODULE, true, "novatek_ts_mp.bin",
					&ts->spi->dev, GFP_KERNEL, ts, nvt_ts_mp_callback);
		return;
	}

	dev_info(&ts->spi->dev, "found firmware novatek_ts_fw.bin (%zu bytes), streaming to SRAM...\n", fw->size);
	mutex_lock(&ts->lock);
	ret = nvt_load_firmware(ts, fw);
	mutex_unlock(&ts->lock);
	if (ret)
		dev_err(&ts->spi->dev, "novatek_ts_fw.bin firmware load failed: %d\n", ret);
	else
		dev_info(&ts->spi->dev, "novatek_ts_fw.bin loaded and IC booted successfully\n");

	release_firmware(fw);
}

static void nvt_ts_hw_reset(struct nvt_ts_data *ts)
{
	if (ts->pinctrl && !IS_ERR_OR_NULL(ts->pin_rst_low))
		pinctrl_select_state(ts->pinctrl, ts->pin_rst_low);
	if (gpio_is_valid(ts->reset_gpio)) {
		gpio_direction_output(ts->reset_gpio, 0);
		gpio_set_value(ts->reset_gpio, 0);
	}
	msleep(20);

	if (ts->pinctrl && !IS_ERR_OR_NULL(ts->pin_rst_high))
		pinctrl_select_state(ts->pinctrl, ts->pin_rst_high);
	if (gpio_is_valid(ts->reset_gpio)) {
		gpio_direction_output(ts->reset_gpio, 1);
		gpio_set_value(ts->reset_gpio, 1);
	}
	msleep(50);
	dev_info(&ts->spi->dev, "hardware reset completed (GPIO %d)\n", ts->reset_gpio);
}

static int nvt_ts_spi_probe(struct spi_device *spi)
{
	struct device_node *np = spi->dev.of_node;
	struct device_node *np_touch = NULL;
	struct platform_device *pdev_touch = NULL;
	struct device *dev_touch = NULL;
	const struct firmware *fw_builtin = NULL;
	struct nvt_ts_data *ts;
	int ret;

	dev_info(&spi->dev, "Novatek NT36xxx SPI touchscreen driver probing...\n");

	ts = devm_kzalloc(&spi->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->spi = spi;
	mutex_init(&ts->lock);

	ts->tx_buf = devm_kzalloc(&spi->dev, 256, GFP_KERNEL);
	ts->rx_buf = devm_kzalloc(&spi->dev, 256, GFP_KERNEL);
	if (!ts->tx_buf || !ts->rx_buf)
		return -ENOMEM;

	ts->max_x = NVT_DEFAULT_X_MAX;
	ts->max_y = NVT_DEFAULT_Y_MAX;
	ts->swrst_n8_addr = NVT_DEF_SWRST_ADDR;
	ts->spi_rd_fast_addr = NVT_DEF_FAST_RD_ADDR;

	/* Parse Device Tree properties from spi child node */
	if (np) {
		ts->reset_gpio = of_get_named_gpio(np, "novatek,reset-gpio", 0);
		if (!gpio_is_valid(ts->reset_gpio))
			ts->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
		if (!gpio_is_valid(ts->reset_gpio))
			ts->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);

		ts->irq_gpio = of_get_named_gpio(np, "novatek,irq-gpio", 0);
		if (!gpio_is_valid(ts->irq_gpio))
			ts->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
		if (!gpio_is_valid(ts->irq_gpio))
			ts->irq_gpio = of_get_named_gpio(np, "irq-gpios", 0);

		of_property_read_u32(np, "novatek,swrst-n8-addr", &ts->swrst_n8_addr);
		of_property_read_u32(np, "novatek,spi-rd-fast-addr", &ts->spi_rd_fast_addr);
	} else {
		ts->reset_gpio = -1;
		ts->irq_gpio = -1;
	}

	/* Check /touch node for overlay properties (mediatek,touch) */
	np_touch = of_find_node_by_path("/touch");
	if (np_touch) {
		u32 res[2];
		pdev_touch = of_find_device_by_node(np_touch);
		if (pdev_touch)
			dev_touch = &pdev_touch->dev;

		if (!gpio_is_valid(ts->reset_gpio))
			ts->reset_gpio = of_get_named_gpio(np_touch, "reset-gpio", 0);
		if (!gpio_is_valid(ts->irq_gpio))
			ts->irq_gpio = of_get_named_gpio(np_touch, "irq-gpio", 0);
		if (of_property_read_u32_array(np_touch, "tpd-resolution", res, 2) == 0) {
			if (res[0] > 0 && res[1] > 0) {
				ts->max_x = res[0] - 1;
				ts->max_y = res[1] - 1;
			}
		}
	}

	/* Fallback to hardware specifications if not specified in DT */
	if (!gpio_is_valid(ts->reset_gpio))
		ts->reset_gpio = NVT_DEFAULT_RESET_GPIO;
	if (!gpio_is_valid(ts->irq_gpio))
		ts->irq_gpio = NVT_DEFAULT_IRQ_GPIO;

	dev_info(&spi->dev, "DT config: reset_gpio=%d, irq_gpio=%d, res=%ux%u, swrst=0x%06x, fast_rd=0x%06x\n",
		 ts->reset_gpio, ts->irq_gpio, ts->max_x, ts->max_y, ts->swrst_n8_addr, ts->spi_rd_fast_addr);

	/* Pinctrl initialization: check /touch platform device first, then spi device */
	if (dev_touch)
		ts->pinctrl = devm_pinctrl_get(dev_touch);
	if (IS_ERR_OR_NULL(ts->pinctrl))
		ts->pinctrl = devm_pinctrl_get(&spi->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl) && spi->dev.parent)
		ts->pinctrl = devm_pinctrl_get(spi->dev.parent);

	if (!IS_ERR_OR_NULL(ts->pinctrl)) {
		ts->pin_spi_mode = pinctrl_lookup_state(ts->pinctrl, "state_spi_mode");
		if (IS_ERR(ts->pin_spi_mode))
			ts->pin_spi_mode = pinctrl_lookup_state(ts->pinctrl, "spimode_default");
		if (IS_ERR(ts->pin_spi_mode))
			ts->pin_spi_mode = pinctrl_lookup_state(ts->pinctrl, "gt9896s_spi_mode");
		if (IS_ERR(ts->pin_spi_mode))
			ts->pin_spi_mode = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_spi_mode");
		if (!IS_ERR_OR_NULL(ts->pin_spi_mode)) {
			ret = pinctrl_select_state(ts->pinctrl, ts->pin_spi_mode);
			dev_info(&spi->dev, "Selected pinctrl state_spi_mode: %d\n", ret);
		}

		ts->pin_active = pinctrl_lookup_state(ts->pinctrl, "state_eint_as_int");
		if (IS_ERR(ts->pin_active))
			ts->pin_active = pinctrl_lookup_state(ts->pinctrl, "eint@0");
		if (IS_ERR(ts->pin_active))
			ts->pin_active = pinctrl_lookup_state(ts->pinctrl, "gt9896s_int_active");
		if (IS_ERR(ts->pin_active))
			ts->pin_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
		if (IS_ERR(ts->pin_active))
			ts->pin_active = pinctrl_lookup_state(ts->pinctrl, PINCTRL_STATE_DEFAULT);
		if (!IS_ERR_OR_NULL(ts->pin_active)) {
			ret = pinctrl_select_state(ts->pinctrl, ts->pin_active);
			dev_info(&spi->dev, "Selected pinctrl state_eint_as_int: %d\n", ret);
		}

		ts->pin_rst_low = pinctrl_lookup_state(ts->pinctrl, "state_rst_output0");
		ts->pin_rst_high = pinctrl_lookup_state(ts->pinctrl, "state_rst_output1");
	}

	/* Power management: enable vtouch regulator if present in DT */
	if (dev_touch)
		ts->vtouch = devm_regulator_get_optional(dev_touch, "vtouch");
	if (IS_ERR_OR_NULL(ts->vtouch))
		ts->vtouch = devm_regulator_get_optional(&spi->dev, "vtouch");

	if (!IS_ERR_OR_NULL(ts->vtouch)) {
		ret = regulator_enable(ts->vtouch);
		if (ret)
			dev_warn(&spi->dev, "regulator_enable vtouch failed: %d\n", ret);
		else
			dev_info(&spi->dev, "vtouch regulator enabled successfully\n");
		msleep(20);
	}

	if (gpio_is_valid(ts->reset_gpio)) {
		ret = devm_gpio_request_one(&spi->dev, ts->reset_gpio, GPIOF_OUT_INIT_HIGH, "nvt_reset_gpio");
		if (ret)
			dev_warn(&spi->dev, "devm_gpio_request_one failed for reset_gpio: %d\n", ret);
	}

	if (gpio_is_valid(ts->irq_gpio)) {
		ret = devm_gpio_request_one(&spi->dev, ts->irq_gpio, GPIOF_IN, "nvt_irq_gpio");
		if (ret)
			dev_warn(&spi->dev, "devm_gpio_request_one failed for irq_gpio: %d\n", ret);
	}

	if (!spi->max_speed_hz)
		spi->max_speed_hz = 8000000;
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi_setup failed: %d\n", ret);
		return ret;
	}

	/*
	 * Non-destructive boot: Check chip identification first.
	 * LK/bootloader already preloaded firmware into volatile SRAM on NO-FLASH chip.
	 * Only perform hardware reset and firmware re-flash if chip identification fails.
	 */
	ret = nvt_check_chip_id(ts);
	if (ret == 0) {
		dev_info(&spi->dev, "Touch IC identified; preserving preloaded SRAM firmware from bootloader\n");
		ts->fw_loaded = true;
	} else {
		dev_warn(&spi->dev, "Touch IC not identified (ret=%d); performing hw reset and firmware reload\n", ret);
		nvt_ts_hw_reset(ts);

		/* Attempt synchronous load first (e.g. built-in firmware in vmlinux) */
		ret = request_firmware(&fw_builtin, "novatek_ts_fw.bin", &spi->dev);
		if (ret == 0 && fw_builtin) {
			dev_info(&spi->dev, "found built-in novatek_ts_fw.bin (%zu bytes), streaming to SRAM...\n", fw_builtin->size);
			mutex_lock(&ts->lock);
			ret = nvt_load_firmware(ts, fw_builtin);
			mutex_unlock(&ts->lock);
			if (ret)
				dev_err(&spi->dev, "built-in firmware load failed: %d\n", ret);
			else
				dev_info(&spi->dev, "built-in novatek_ts_fw.bin loaded and IC booted successfully\n");
			release_firmware(fw_builtin);
		} else {
			/* Fallback to asynchronous request via userspace ueventd */
			dev_info(&spi->dev, "requesting novatek_ts_fw.bin asynchronously...\n");
			request_firmware_nowait(THIS_MODULE, true, "novatek_ts_fw.bin",
						&spi->dev, GFP_KERNEL, ts, nvt_ts_fw_callback);
		}
	}

	/* Standalone input device named "mtk-tpd" matching exact stock contract */
	ts->input_dev = devm_input_allocate_device(&spi->dev);
	if (!ts->input_dev)
		return -ENOMEM;

	ts->input_dev->name = NVT_TS_NAME;
	ts->input_dev->id.bustype = BUS_SPI;
	ts->input_dev->dev.parent = &spi->dev;

	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(EV_ABS, ts->input_dev->evbit);

	__set_bit(BTN_TOUCH, ts->input_dev->keybit);
	__set_bit(KEY_F13, ts->input_dev->keybit);
	__set_bit(KEY_F14, ts->input_dev->keybit);

	input_set_abs_params(ts->input_dev, ABS_X, 0, 1080, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, 2460, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_SLOT, 0, 9, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, 1079, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, 2459, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 1000, 0, 0);

	ret = input_mt_init_slots(ts->input_dev, NVT_MAX_TOUCH_POINTS, INPUT_MT_DIRECT);
	if (ret) {
		dev_err(&spi->dev, "input_mt_init_slots failed: %d\n", ret);
		return ret;
	}

	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&spi->dev, "input_register_device failed: %d\n", ret);
		return ret;
	}

	if (gpio_is_valid(ts->irq_gpio))
		ts->irq = gpio_to_irq(ts->irq_gpio);
	if (ts->irq <= 0 && spi->irq > 0)
		ts->irq = spi->irq;
	if (ts->irq <= 0 && np_touch)
		ts->irq = irq_of_parse_and_map(np_touch, 0);

	if (ts->irq <= 0) {
		dev_err(&spi->dev, "Invalid IRQ number: %d\n", ts->irq);
		return -EINVAL;
	}

	/* Matching hardware INT line: interrupts = <0xE 0x2> -> IRQF_TRIGGER_FALLING | IRQF_ONESHOT */
	ret = devm_request_threaded_irq(&spi->dev, ts->irq, NULL, nvt_ts_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"NVT-ts", ts);
	if (ret) {
		dev_err(&spi->dev, "request_threaded_irq failed on %d: %d\n", ts->irq, ret);
		return ret;
	}

	spi_set_drvdata(spi, ts);

	dev_info(&spi->dev, "Novatek NT36xxx SPI touchscreen registered successfully (IRQ %d)\n", ts->irq);
	return 0;
}

static const struct of_device_id nvt_ts_spi_of_match[] = {
	{ .compatible = "novatek,NVT-ts-spi" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nvt_ts_spi_of_match);

static const struct spi_device_id nvt_ts_spi_id[] = {
	{ "NVT-ts-spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, nvt_ts_spi_id);

static struct spi_driver nvt_ts_spi_driver = {
	.driver = {
		.name           = NVT_DRIVER_NAME,
		.of_match_table = nvt_ts_spi_of_match,
	},
	.probe  = nvt_ts_spi_probe,
	.id_table = nvt_ts_spi_id,
};

module_spi_driver(nvt_ts_spi_driver);

MODULE_AUTHOR("rag12ghu-design");
MODULE_DESCRIPTION("Novatek NT36xxx SPI Touchscreen Driver");
MODULE_LICENSE("GPL v2");
