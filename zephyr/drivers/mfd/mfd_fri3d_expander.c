/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * CH32X035 I/O coprocessor of the Fri3d Camp 2026 badge (badge_2026_fw).
 *
 * With CONFIG_MFD_FRI3D_EXPANDER_UPDATE its firmware can be updated over
 * RVSWD (mfd_fri3d_expander_update.c).
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander

#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "mfd_fri3d_expander_priv.h"

LOG_MODULE_REGISTER(mfd_fri3d_expander, CONFIG_MFD_LOG_LEVEL);


int mfd_fri3d_expander_reg_read(const struct device *dev, uint8_t reg, void *data, size_t len)
{
	const struct mfd_fri3d_expander_config *config = dev->config;

	return i2c_write_read_dt(&config->i2c, &reg, sizeof(reg), data, len);
}

int mfd_fri3d_expander_reg_read16(const struct device *dev, uint8_t reg, uint16_t *val)
{
	uint8_t buf[2];
	int ret;

	ret = mfd_fri3d_expander_reg_read(dev, reg, buf, sizeof(buf));
	if (ret == 0) {
		*val = sys_get_le16(buf);
	}

	return ret;
}

int mfd_fri3d_expander_reg_write16(const struct device *dev, uint8_t reg, uint16_t val)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	uint8_t buf[3] = {reg};

	sys_put_le16(val, &buf[1]);

	return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

int mfd_fri3d_expander_outputs_update(const struct device *dev, uint8_t mask, uint8_t val)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	struct mfd_fri3d_expander_data *data = dev->data;
	uint8_t outputs;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	outputs = (data->outputs & ~mask) | (val & mask);
	ret = i2c_reg_write_byte_dt(&config->i2c, FRI3D_EXPANDER_REG_OUTPUTS, outputs);
	if (ret == 0) {
		data->outputs = outputs;
	}
	k_mutex_unlock(&data->lock);

	return ret;
}

uint8_t mfd_fri3d_expander_outputs_get(const struct device *dev)
{
	struct mfd_fri3d_expander_data *data = dev->data;

	return data->outputs;
}

void mfd_fri3d_expander_get_version(const struct device *dev, uint8_t version[3])
{
	struct mfd_fri3d_expander_data *data = dev->data;

	memcpy(version, data->version, sizeof(data->version));
}

uint16_t mfd_fri3d_expander_get_inputs(const struct device *dev)
{
	struct mfd_fri3d_expander_data *data = dev->data;

	return data->inputs;
}

uint32_t mfd_fri3d_expander_get_int_count(const struct device *dev)
{
	struct mfd_fri3d_expander_data *data = dev->data;

	return data->int_count;
}

int mfd_fri3d_expander_add_callback(const struct device *dev, struct gpio_callback *callback)
{
	struct mfd_fri3d_expander_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = gpio_manage_callback(&data->callbacks, callback, true);
	k_mutex_unlock(&data->lock);

	return ret;
}

int mfd_fri3d_expander_remove_callback(const struct device *dev, struct gpio_callback *callback)
{
	struct mfd_fri3d_expander_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = gpio_manage_callback(&data->callbacks, callback, false);
	k_mutex_unlock(&data->lock);

	return ret;
}

/* Read the inputs and notify the children of the bits that changed */
void mfd_fri3d_expander_update_inputs(const struct device *dev)
{
	struct mfd_fri3d_expander_data *data = dev->data;
	uint16_t inputs;
	uint16_t changed;

	if (data->updating) {
		return;
	}

	if (mfd_fri3d_expander_reg_read16(dev, FRI3D_EXPANDER_REG_INPUT, &inputs) != 0) {
		LOG_ERR("%s: failed to read the inputs", dev->name);
		return;
	}

	changed = inputs ^ data->inputs;
	data->inputs = inputs;

	if (changed != 0) {
		gpio_fire_callbacks(&data->callbacks, dev, changed);
	}
}

#ifdef CONFIG_MFD_FRI3D_EXPANDER_POLL

static void mfd_fri3d_expander_poll_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct mfd_fri3d_expander_data *data =
		CONTAINER_OF(dwork, struct mfd_fri3d_expander_data, poll_work);

	mfd_fri3d_expander_update_inputs(data->dev);
	k_work_reschedule(dwork, K_MSEC(CONFIG_MFD_FRI3D_EXPANDER_POLL_PERIOD_MS));
}

#else

static void mfd_fri3d_expander_int_work(struct k_work *work)
{
	struct mfd_fri3d_expander_data *data =
		CONTAINER_OF(work, struct mfd_fri3d_expander_data, int_work);

	data->int_count++;
	mfd_fri3d_expander_update_inputs(data->dev);
}

static void mfd_fri3d_expander_int_handler(const struct device *port, struct gpio_callback *cb,
					   uint32_t pins)
{
	struct mfd_fri3d_expander_data *data =
		CONTAINER_OF(cb, struct mfd_fri3d_expander_data, int_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	k_work_submit(&data->int_work);
}

static int mfd_fri3d_expander_init_int(const struct device *dev)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	struct mfd_fri3d_expander_data *data = dev->data;
	int ret;

	if (config->int_gpio.port == NULL) {
		return 0;
	}

	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("%s: INT GPIO not ready", dev->name);
		return -ENODEV;
	}

	k_work_init(&data->int_work, mfd_fri3d_expander_int_work);

	ret = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT);
	if (ret != 0) {
		return ret;
	}

	gpio_init_callback(&data->int_cb, mfd_fri3d_expander_int_handler,
			   BIT(config->int_gpio.pin));
	ret = gpio_add_callback_dt(&config->int_gpio, &data->int_cb);
	if (ret != 0) {
		return ret;
	}

	/* the coprocessor pulses INT (about 50 ms) when an input changes */
	return gpio_pin_interrupt_configure_dt(&config->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
}

#endif /* CONFIG_MFD_FRI3D_EXPANDER_POLL */

int mfd_fri3d_expander_wait(const struct device *dev, k_timepoint_t timeout)
{
	struct mfd_fri3d_expander_data *data = dev->data;

	while (mfd_fri3d_expander_reg_read(dev, FRI3D_EXPANDER_REG_VERSION, data->version,
					   sizeof(data->version)) != 0) {
		if (sys_timepoint_expired(timeout)) {
			return -ETIMEDOUT;
		}
		k_msleep(50);
	}

	return 0;
}


static int mfd_fri3d_expander_init(const struct device *dev)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	struct mfd_fri3d_expander_data *data = dev->data;
	k_timepoint_t timeout = sys_timepoint_calc(K_MSEC(config->boot_timeout_ms));
	int ret;

	data->dev = dev;
	k_mutex_init(&data->lock);

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("%s: I2C bus not ready", dev->name);
		return -ENODEV;
	}

	/* After a power-on the coprocessor waits a second before it enables I2C */
	ret = mfd_fri3d_expander_wait(dev, timeout);
	if (ret == 0) {
		ret = mfd_fri3d_expander_reg_read(dev, FRI3D_EXPANDER_REG_OUTPUTS, &data->outputs,
						  sizeof(data->outputs));
		data->outputs_valid = ret == 0;
	}

#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE_AT_BOOT
	ret = mfd_fri3d_expander_update_at_boot(dev, ret);
#endif
	if (ret != 0) {
		LOG_ERR("%s: no answer from the coprocessor", dev->name);
		return -ENODEV;
	}

	LOG_INF("%s: firmware v%u.%u.%u", dev->name, data->version[0], data->version[1],
		data->version[2]);

	ret = mfd_fri3d_expander_reg_read16(dev, FRI3D_EXPANDER_REG_INPUT, &data->inputs);
	if (ret != 0) {
		LOG_ERR("%s: failed to read the registers: %d", dev->name, ret);
		return ret;
	}

#ifdef CONFIG_MFD_FRI3D_EXPANDER_POLL
	k_work_init_delayable(&data->poll_work, mfd_fri3d_expander_poll_work);
	k_work_schedule(&data->poll_work, K_MSEC(CONFIG_MFD_FRI3D_EXPANDER_POLL_PERIOD_MS));
	return 0;
#else
	return mfd_fri3d_expander_init_int(dev);
#endif
}

#define MFD_FRI3D_EXPANDER_DEFINE(inst)                                                            \
	static const struct mfd_fri3d_expander_config mfd_fri3d_expander_config##inst = {          \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.int_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, int_gpios, {0}),                        \
		.boot_timeout_ms = DT_INST_PROP(inst, boot_timeout_ms),                            \
		IF_ENABLED(CONFIG_MFD_FRI3D_EXPANDER_UPDATE,                                       \
			   (.rvswd = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, rvswd),               \
						 (DEVICE_DT_GET(DT_INST_PHANDLE(inst, rvswd))),     \
						 (NULL)),))                                         \
	};                                                                                         \
	static struct mfd_fri3d_expander_data mfd_fri3d_expander_data##inst;                       \
	DEVICE_DT_INST_DEFINE(inst, mfd_fri3d_expander_init, NULL,                                 \
			      &mfd_fri3d_expander_data##inst, &mfd_fri3d_expander_config##inst,    \
			      POST_KERNEL, CONFIG_MFD_FRI3D_EXPANDER_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_FRI3D_EXPANDER_DEFINE)
