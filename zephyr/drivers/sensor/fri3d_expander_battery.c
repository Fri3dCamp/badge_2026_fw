/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Battery voltage of the Fri3d Camp 2026 badge, measured by the I/O
 * coprocessor (12 bit ADC behind a 100k/100k divider).
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander_sensor

#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fri3d_expander_battery, CONFIG_SENSOR_LOG_LEVEL);

struct fri3d_expander_battery_config {
	const struct device *mfd;
};

struct fri3d_expander_battery_data {
	int32_t battery_mv;
};

/* Calibrated by MicroPythonOS: V = raw * 0.00192308 - 0.28076923 */
static int32_t fri3d_expander_battery_mv(uint16_t raw)
{
	return ((int32_t)raw * 19231) / 10000 - 281;
}

static int fri3d_expander_battery_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct fri3d_expander_battery_config *config = dev->config;
	struct fri3d_expander_battery_data *data = dev->data;
	uint16_t raw;
	int ret;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	ret = mfd_fri3d_expander_reg_read16(config->mfd, FRI3D_EXPANDER_REG_BATTERY, &raw);
	if (ret != 0) {
		return ret;
	}

	data->battery_mv = MAX(fri3d_expander_battery_mv(raw), 0);

	return 0;
}

static int fri3d_expander_battery_channel_get(const struct device *dev, enum sensor_channel chan,
					      struct sensor_value *val)
{
	struct fri3d_expander_battery_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	return sensor_value_from_milli(val, data->battery_mv);
}

static DEVICE_API(sensor, fri3d_expander_battery_api) = {
	.sample_fetch = fri3d_expander_battery_sample_fetch,
	.channel_get = fri3d_expander_battery_channel_get,
};

static int fri3d_expander_battery_init(const struct device *dev)
{
	const struct fri3d_expander_battery_config *config = dev->config;

	if (!device_is_ready(config->mfd)) {
		LOG_ERR("%s: expander not ready", dev->name);
		return -ENODEV;
	}

	return 0;
}

#define FRI3D_EXPANDER_BATTERY_DEFINE(inst)                                                        \
	static const struct fri3d_expander_battery_config fri3d_expander_battery_config##inst = {  \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
	};                                                                                         \
	static struct fri3d_expander_battery_data fri3d_expander_battery_data##inst;               \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, fri3d_expander_battery_init, NULL,                      \
				     &fri3d_expander_battery_data##inst,                           \
				     &fri3d_expander_battery_config##inst, POST_KERNEL,            \
				     CONFIG_SENSOR_INIT_PRIORITY, &fri3d_expander_battery_api);

DT_INST_FOREACH_STATUS_OKAY(FRI3D_EXPANDER_BATTERY_DEFINE)
