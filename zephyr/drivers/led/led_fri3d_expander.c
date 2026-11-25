/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PWM outputs of the Fri3d Camp 2026 badge I/O coprocessor: LED 0 is the LCD
 * backlight, LED 1 the debug LED.
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander_led

#include <zephyr/drivers/led.h>
#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_fri3d_expander, CONFIG_LED_LOG_LEVEL);

static const uint8_t led_fri3d_expander_regs[] = {
	FRI3D_EXPANDER_REG_LCD_BRIGHTNESS, /* 0: LCD backlight */
	FRI3D_EXPANDER_REG_LED_BRIGHTNESS, /* 1: debug LED */
};

struct led_fri3d_expander_config {
	const struct device *mfd;
};

static int led_fri3d_expander_set_brightness(const struct device *dev, uint32_t led,
					     uint8_t value)
{
	const struct led_fri3d_expander_config *config = dev->config;

	if (led >= ARRAY_SIZE(led_fri3d_expander_regs)) {
		return -EINVAL;
	}

	return mfd_fri3d_expander_reg_write16(config->mfd, led_fri3d_expander_regs[led],
					      MIN(value, LED_BRIGHTNESS_MAX));
}

static DEVICE_API(led, led_fri3d_expander_api) = {
	.set_brightness = led_fri3d_expander_set_brightness,
};

static int led_fri3d_expander_init(const struct device *dev)
{
	const struct led_fri3d_expander_config *config = dev->config;

	if (!device_is_ready(config->mfd)) {
		LOG_ERR("%s: expander not ready", dev->name);
		return -ENODEV;
	}

	return 0;
}

#define LED_FRI3D_EXPANDER_DEFINE(inst)                                                            \
	static const struct led_fri3d_expander_config led_fri3d_expander_config##inst = {          \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, led_fri3d_expander_init, NULL, NULL,                           \
			      &led_fri3d_expander_config##inst, POST_KERNEL,                       \
			      CONFIG_LED_FRI3D_EXPANDER_INIT_PRIORITY, &led_fri3d_expander_api);

DT_INST_FOREACH_STATUS_OKAY(LED_FRI3D_EXPANDER_DEFINE)
