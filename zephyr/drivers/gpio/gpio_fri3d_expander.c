/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Output-only GPIO controller of the Fri3d Camp 2026 badge I/O coprocessor:
 * pin N is bit N of its output register (AUX 3V3, LCD reset, LoRa reset).
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander_gpio

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gpio_fri3d_expander, CONFIG_GPIO_LOG_LEVEL);

/* Output register bits that are pin levels (the others are commands) */
#define GPIO_FRI3D_EXPANDER_PINS                                                                   \
	(FRI3D_EXPANDER_OUT_AUX_3V3 | FRI3D_EXPANDER_OUT_LCD_RESET | FRI3D_EXPANDER_OUT_LORA_RESET)

struct gpio_fri3d_expander_config {
	struct gpio_driver_config common;
	const struct device *mfd;
};

struct gpio_fri3d_expander_data {
	struct gpio_driver_data common;
};

static int gpio_fri3d_expander_pin_configure(const struct device *dev, gpio_pin_t pin,
					     gpio_flags_t flags)
{
	const struct gpio_fri3d_expander_config *config = dev->config;

	if ((BIT(pin) & GPIO_FRI3D_EXPANDER_PINS) == 0) {
		return -EINVAL;
	}

	if ((flags & (GPIO_INPUT | GPIO_SINGLE_ENDED | GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0) {
		return -ENOTSUP;
	}

	if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0) {
		return mfd_fri3d_expander_outputs_update(config->mfd, BIT(pin), BIT(pin));
	}

	if ((flags & GPIO_OUTPUT_INIT_LOW) != 0) {
		return mfd_fri3d_expander_outputs_update(config->mfd, BIT(pin), 0);
	}

	return 0;
}

static int gpio_fri3d_expander_port_get_raw(const struct device *dev, gpio_port_value_t *value)
{
	const struct gpio_fri3d_expander_config *config = dev->config;

	/* output-only: report the levels that were last written */
	*value = mfd_fri3d_expander_outputs_get(config->mfd) & GPIO_FRI3D_EXPANDER_PINS;

	return 0;
}

static int gpio_fri3d_expander_port_set_masked_raw(const struct device *dev,
						   gpio_port_pins_t mask, gpio_port_value_t value)
{
	const struct gpio_fri3d_expander_config *config = dev->config;

	return mfd_fri3d_expander_outputs_update(config->mfd, mask & GPIO_FRI3D_EXPANDER_PINS,
						 value);
}

static int gpio_fri3d_expander_port_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	return gpio_fri3d_expander_port_set_masked_raw(dev, pins, pins);
}

static int gpio_fri3d_expander_port_clear_bits_raw(const struct device *dev,
						   gpio_port_pins_t pins)
{
	return gpio_fri3d_expander_port_set_masked_raw(dev, pins, 0);
}

static int gpio_fri3d_expander_port_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_fri3d_expander_config *config = dev->config;

	return gpio_fri3d_expander_port_set_masked_raw(
		dev, pins, ~mfd_fri3d_expander_outputs_get(config->mfd));
}

static DEVICE_API(gpio, gpio_fri3d_expander_api) = {
	.pin_configure = gpio_fri3d_expander_pin_configure,
	.port_get_raw = gpio_fri3d_expander_port_get_raw,
	.port_set_masked_raw = gpio_fri3d_expander_port_set_masked_raw,
	.port_set_bits_raw = gpio_fri3d_expander_port_set_bits_raw,
	.port_clear_bits_raw = gpio_fri3d_expander_port_clear_bits_raw,
	.port_toggle_bits = gpio_fri3d_expander_port_toggle_bits,
};

static int gpio_fri3d_expander_init(const struct device *dev)
{
	const struct gpio_fri3d_expander_config *config = dev->config;

	if (!device_is_ready(config->mfd)) {
		LOG_ERR("%s: expander not ready", dev->name);
		return -ENODEV;
	}

	return 0;
}

#define GPIO_FRI3D_EXPANDER_DEFINE(inst)                                                           \
	static const struct gpio_fri3d_expander_config gpio_fri3d_expander_config##inst = {        \
		.common = GPIO_COMMON_CONFIG_FROM_DT_INST(inst),                                   \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
	};                                                                                         \
	static struct gpio_fri3d_expander_data gpio_fri3d_expander_data##inst;                     \
	DEVICE_DT_INST_DEFINE(inst, gpio_fri3d_expander_init, NULL,                                \
			      &gpio_fri3d_expander_data##inst, &gpio_fri3d_expander_config##inst,  \
			      POST_KERNEL, CONFIG_GPIO_FRI3D_EXPANDER_INIT_PRIORITY,               \
			      &gpio_fri3d_expander_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_FRI3D_EXPANDER_DEFINE)
