// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2022 MediaTek Inc.
 * External Camera PMIC / LDO driver for MT6877 / X6815C
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/upmu_common.h>

extern unsigned int pmic_config_interface(unsigned int RegNum,
					  unsigned int val,
					  unsigned int MASK,
					  unsigned int SHIFT);

#include "regulator.h"
#include "imgsensor_cfg_table.h"

#define EXT_LDO_I2C_NAME "cam_vdd_ext"

static DEFINE_MUTEX(ext_ldo_mutex);
static DEFINE_MUTEX(ext_ldo_pinctrl_lock);
static struct i2c_client *ext_ldo_i2c_client;
static struct pinctrl *ext_ldo_pinctrl;
static struct pinctrl_state *ext_ldo_hwen_high;
static struct pinctrl_state *ext_ldo_hwen_low;
static struct work_struct ext_ldo_work;
static struct hrtimer ext_ldo_timer;
static u8 ext_ldo_current_enable_mask;

static void ext_ldo_work_disable(struct work_struct *work)
{
	mutex_lock(&ext_ldo_pinctrl_lock);
	if (ext_ldo_pinctrl && ext_ldo_hwen_low)
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_low);
	mutex_unlock(&ext_ldo_pinctrl_lock);

	mutex_lock(&ext_ldo_mutex);
	if (ext_ldo_i2c_client) {
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x0E, 0x00);
		ext_ldo_current_enable_mask = 0;
	}
	mutex_unlock(&ext_ldo_mutex);
	pr_info("[ext_ldo] standby entered\n");
}

static enum hrtimer_restart ext_ldo_timer_func(struct hrtimer *timer)
{
	schedule_work(&ext_ldo_work);
	return HRTIMER_NORESTART;
}

int ext_ldo_enable(void)
{
	int ret = 0;

	mutex_lock(&ext_ldo_pinctrl_lock);
	if (ext_ldo_pinctrl && ext_ldo_hwen_high)
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_high);
	mutex_unlock(&ext_ldo_pinctrl_lock);

	mutex_lock(&ext_ldo_mutex);
	if (!ext_ldo_i2c_client) {
		mutex_unlock(&ext_ldo_mutex);
		return -ENODEV;
	}

	pmic_config_interface(0x1888, 1, 1, 0);

	ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x0E, ext_ldo_current_enable_mask);
	mutex_unlock(&ext_ldo_mutex);
	return ret;
}
EXPORT_SYMBOL(ext_ldo_enable);

int ext_ldo_disable(void)
{
	int ret = 0;

	hrtimer_cancel(&ext_ldo_timer);

	mutex_lock(&ext_ldo_mutex);
	if (ext_ldo_i2c_client) {
		ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x0E, 0x00);
		ext_ldo_current_enable_mask = 0;
	}
	mutex_unlock(&ext_ldo_mutex);

	mutex_lock(&ext_ldo_pinctrl_lock);
	if (ext_ldo_pinctrl && ext_ldo_hwen_low)
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_low);
	mutex_unlock(&ext_ldo_pinctrl_lock);

	return ret;
}
EXPORT_SYMBOL(ext_ldo_disable);

int single_ldo_control(int pin, int voltage)
{
	u8 val = 0;
	u8 mask_bit = 0;
	int ret = 0;

	hrtimer_cancel(&ext_ldo_timer);

	mutex_lock(&ext_ldo_pinctrl_lock);
	if (ext_ldo_pinctrl && ext_ldo_hwen_high)
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_high);
	mutex_unlock(&ext_ldo_pinctrl_lock);

	mutex_lock(&ext_ldo_mutex);
	if (!ext_ldo_i2c_client) {
		pr_err("[ext_ldo] single_ldo_control: i2c_client is NULL!\n");
		mutex_unlock(&ext_ldo_mutex);
		return -ENODEV;
	}

	pmic_config_interface(0x1888, 1, 1, 0);

	switch (pin) {
	case IMGSENSOR_HW_PIN_AVDD: /* Pin 3 (vcama) */
		if (voltage < 1200000)
			val = 0;
		else
			val = (u8)((voltage - 1200000) / 12500);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x05, val);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x02, 0x04);
		mask_bit = 0x04;
		pr_info("[ext_ldo] vcama (pin 3) set to %d uV (reg05=0x%02x, bit 0x04)\n", voltage, val);
		break;

	case IMGSENSOR_HW_PIN_DVDD: /* Pin 6 (vcamd) */
		if (voltage < 600000)
			val = 0;
		else
			val = (u8)(((u64)(voltage - 600000) * 0x057619f1ULL) >> 39);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x03, val);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x02, 0x01);
		mask_bit = 0x01;
		pr_info("[ext_ldo] vcamd (pin 6) set to %d uV (reg03=0x%02x, bit 0x01)\n", voltage, val);
		break;

	case IMGSENSOR_HW_PIN_EXTA2: /* Pin 7 (exta2) */
		if (voltage < 1200000)
			val = 0;
		else
			val = (u8)((voltage - 1200000) / 12500);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x06, val);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x02, 0x04);
		mask_bit = 0x08;
		pr_info("[ext_ldo] exta2 (pin 7) set to %d uV (reg06=0x%02x, bit 0x08)\n", voltage, val);
		break;

	case IMGSENSOR_HW_PIN_EXTD2: /* Pin 8 (extd2) */
		if (voltage < 600000)
			val = 0;
		else
			val = (u8)(((u64)(voltage - 600000) * 0x057619f1ULL) >> 39);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x04, val);
		i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x02, 0x02);
		mask_bit = 0x02;
		pr_info("[ext_ldo] extd2 (pin 8) set to %d uV (reg04=0x%02x, bit 0x02)\n", voltage, val);
		break;

	default:
		pr_debug("[ext_ldo] unhandled rail pin %d\n", pin);
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	ext_ldo_current_enable_mask |= mask_bit;
	ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x0E, ext_ldo_current_enable_mask);
	pr_info("[ext_ldo] enabled mask now 0x%02x (ret=%d)\n", ext_ldo_current_enable_mask, ret);

	mutex_unlock(&ext_ldo_mutex);
	return ret;
}
EXPORT_SYMBOL(single_ldo_control);

int single_ldo_disable(int pin)
{
	u8 clear_mask = 0;
	int ret = 0;

	mutex_lock(&ext_ldo_mutex);
	if (!ext_ldo_i2c_client) {
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	switch (pin) {
	case IMGSENSOR_HW_PIN_AVDD:  /* Pin 3 (vcama) */
		clear_mask = 0x04;
		break;
	case IMGSENSOR_HW_PIN_DVDD:  /* Pin 6 (vcamd) */
		clear_mask = 0x01;
		break;
	case IMGSENSOR_HW_PIN_EXTA2: /* Pin 7 (exta2) */
		clear_mask = 0x08;
		break;
	case IMGSENSOR_HW_PIN_EXTD2: /* Pin 8 (extd2) */
		clear_mask = 0x02;
		break;
	default:
		mutex_unlock(&ext_ldo_mutex);
		return 0;
	}

	ext_ldo_current_enable_mask &= ~clear_mask;
	ret = i2c_smbus_write_byte_data(ext_ldo_i2c_client, 0x0E, ext_ldo_current_enable_mask);
	pr_info("[ext_ldo] disabled pin %d, mask now 0x%02x\n", pin, ext_ldo_current_enable_mask);

	if (ext_ldo_current_enable_mask == 0) {
		/* Start timer for delayed standby (5 seconds) */
		hrtimer_start(&ext_ldo_timer, ktime_set(5, 0), HRTIMER_MODE_REL);
	}

	mutex_unlock(&ext_ldo_mutex);
	return ret;
}
EXPORT_SYMBOL(single_ldo_disable);

static int ext_ldo_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	pr_info("[ext_ldo] ext_ldo_i2c_probe: addr 0x%02x on adapter %s\n", client->addr, client->adapter->name);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("[ext_ldo] i2c_check_functionality failed!\n");
		return -ENODEV;
	}

	mutex_lock(&ext_ldo_pinctrl_lock);
	if (ext_ldo_pinctrl && ext_ldo_hwen_high)
		pinctrl_select_state(ext_ldo_pinctrl, ext_ldo_hwen_high);
	mutex_unlock(&ext_ldo_pinctrl_lock);

	pmic_config_interface(0x1888, 1, 1, 0);

	/* Initialize all channels to off */
	ret = i2c_smbus_write_byte_data(client, 0x0E, 0x00);
	if (ret < 0) {
		pr_err("[ext_ldo] failed initial write to reg 0x0E (%d)\n", ret);
	}

	mutex_lock(&ext_ldo_mutex);
	ext_ldo_i2c_client = client;
	ext_ldo_current_enable_mask = 0;
	mutex_unlock(&ext_ldo_mutex);

	INIT_WORK(&ext_ldo_work, ext_ldo_work_disable);
	hrtimer_init(&ext_ldo_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ext_ldo_timer.function = ext_ldo_timer_func;

	pr_info("[ext_ldo] ext_ldo_i2c_probe success\n");
	return 0;
}

static int ext_ldo_i2c_remove(struct i2c_client *client)
{
	hrtimer_cancel(&ext_ldo_timer);
	cancel_work_sync(&ext_ldo_work);

	mutex_lock(&ext_ldo_mutex);
	if (ext_ldo_i2c_client == client)
		ext_ldo_i2c_client = NULL;
	mutex_unlock(&ext_ldo_mutex);

	return 0;
}

static const struct i2c_device_id ext_ldo_i2c_id[] = {
	{ EXT_LDO_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ext_ldo_i2c_id);

static const struct of_device_id ext_ldo_i2c_of_match[] = {
	{ .compatible = "mediatek,cam_vdd_ext", },
	{ }
};
MODULE_DEVICE_TABLE(of, ext_ldo_i2c_of_match);

static struct i2c_driver ext_ldo_i2c_driver = {
	.driver = {
		.name = EXT_LDO_I2C_NAME,
		.of_match_table = ext_ldo_i2c_of_match,
	},
	.probe = ext_ldo_i2c_probe,
	.remove = ext_ldo_i2c_remove,
	.id_table = ext_ldo_i2c_id,
};

static int ext_ldo_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[ext_ldo] ext_ldo_probe platform device\n");

	ext_ldo_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ext_ldo_pinctrl)) {
		pr_err("[ext_ldo] devm_pinctrl_get failed\n");
		ext_ldo_pinctrl = NULL;
	} else {
		ext_ldo_hwen_high = pinctrl_lookup_state(ext_ldo_pinctrl, "extldo_en_high");
		if (IS_ERR(ext_ldo_hwen_high))
			ext_ldo_hwen_high = pinctrl_lookup_state(ext_ldo_pinctrl, "ext_ldo_pins_hwen_high");
		if (IS_ERR(ext_ldo_hwen_high)) {
			pr_warn("[ext_ldo] extldo_en_high not found\n");
			ext_ldo_hwen_high = NULL;
		}

		ext_ldo_hwen_low = pinctrl_lookup_state(ext_ldo_pinctrl, "extldo_en_low");
		if (IS_ERR(ext_ldo_hwen_low))
			ext_ldo_hwen_low = pinctrl_lookup_state(ext_ldo_pinctrl, "ext_ldo_pins_hwen_low");
		if (IS_ERR(ext_ldo_hwen_low)) {
			pr_warn("[ext_ldo] extldo_en_low not found\n");
			ext_ldo_hwen_low = NULL;
		}
	}

	ret = i2c_add_driver(&ext_ldo_i2c_driver);
	if (ret)
		pr_err("[ext_ldo] i2c_add_driver failed: %d\n", ret);

	return ret;
}

static int ext_ldo_remove(struct platform_device *pdev)
{
	i2c_del_driver(&ext_ldo_i2c_driver);
	return 0;
}

static const struct of_device_id ext_ldo_of_match[] = {
	{ .compatible = "mediatek,cam_vdd_ext", },
	{ }
};
MODULE_DEVICE_TABLE(of, ext_ldo_of_match);

static struct platform_driver ext_ldo_platform_driver = {
	.probe = ext_ldo_probe,
	.remove = ext_ldo_remove,
	.driver = {
		.name = "cam_vdd_ext",
		.of_match_table = ext_ldo_of_match,
	},
};

static int __init ext_ldo_init(void)
{
	return platform_driver_register(&ext_ldo_platform_driver);
}

static void __exit ext_ldo_exit(void)
{
	platform_driver_unregister(&ext_ldo_platform_driver);
}

subsys_initcall(ext_ldo_init);
module_exit(ext_ldo_exit);

MODULE_DESCRIPTION("MediaTek External Camera LDO Driver");
MODULE_LICENSE("GPL v2");
