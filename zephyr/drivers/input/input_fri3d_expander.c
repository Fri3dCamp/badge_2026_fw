/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Buttons and joystick directions of the Fri3d Camp 2026 badge, read by the
 * CH32X035 I/O coprocessor.
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander_keys

#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(input_fri3d_expander, CONFIG_INPUT_LOG_LEVEL);

struct input_fri3d_expander_key {
	uint16_t bit;
	uint16_t code;
};

struct input_fri3d_expander_config {
	const struct device *mfd;
	const struct input_fri3d_expander_key *keys;
	size_t num_keys;
};

struct input_fri3d_expander_data {
	const struct device *dev;
	struct gpio_callback cb;
};

static void input_fri3d_expander_changed(const struct device *mfd, struct gpio_callback *cb,
					 uint32_t changed)
{
	struct input_fri3d_expander_data *data =
		CONTAINER_OF(cb, struct input_fri3d_expander_data, cb);
	const struct device *dev = data->dev;
	const struct input_fri3d_expander_config *config = dev->config;
	uint16_t inputs = mfd_fri3d_expander_get_inputs(mfd);
	size_t last = SIZE_MAX;

	/* the last changed key gets sync=true */
	for (size_t i = 0; i < config->num_keys; i++) {
		if (changed & config->keys[i].bit) {
			last = i;
		}
	}

	for (size_t i = 0; i < config->num_keys; i++) {
		const struct input_fri3d_expander_key *key = &config->keys[i];

		if (changed & key->bit) {
			input_report_key(dev, key->code, (inputs & key->bit) ? 1 : 0, i == last,
					 K_FOREVER);
		}
	}
}

static int input_fri3d_expander_init(const struct device *dev)
{
	const struct input_fri3d_expander_config *config = dev->config;
	struct input_fri3d_expander_data *data = dev->data;
	gpio_port_pins_t mask = 0;

	if (!device_is_ready(config->mfd)) {
		LOG_ERR("%s: expander not ready", dev->name);
		return -ENODEV;
	}

	for (size_t i = 0; i < config->num_keys; i++) {
		mask |= config->keys[i].bit;
	}

	data->dev = dev;
	gpio_init_callback(&data->cb, input_fri3d_expander_changed, mask);

	return mfd_fri3d_expander_add_callback(config->mfd, &data->cb);
}

#define INPUT_FRI3D_EXPANDER_KEY_ENTRY(node_id)                                                    \
	{                                                                                          \
		.bit = BIT(DT_PROP(node_id, bit_num)),                                             \
		.code = DT_PROP(node_id, zephyr_code),                                             \
	}

#define INPUT_FRI3D_EXPANDER_KEY_CHECK(node_id)                                                    \
	BUILD_ASSERT(DT_PROP(node_id, bit_num) < 16, "bit-num must be 0-15");

#define INPUT_FRI3D_EXPANDER_DEFINE(inst)                                                          \
	DT_INST_FOREACH_CHILD_STATUS_OKAY(inst, INPUT_FRI3D_EXPANDER_KEY_CHECK)                    \
                                                                                                   \
	static const struct input_fri3d_expander_key input_fri3d_expander_keys##inst[] = {         \
		DT_INST_FOREACH_CHILD_STATUS_OKAY_SEP(inst, INPUT_FRI3D_EXPANDER_KEY_ENTRY, (,))}; \
                                                                                                   \
	static const struct input_fri3d_expander_config input_fri3d_expander_config##inst = {      \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
		.keys = input_fri3d_expander_keys##inst,                                           \
		.num_keys = ARRAY_SIZE(input_fri3d_expander_keys##inst),                           \
	};                                                                                         \
	static struct input_fri3d_expander_data input_fri3d_expander_data##inst;                   \
	DEVICE_DT_INST_DEFINE(inst, input_fri3d_expander_init, NULL,                               \
			      &input_fri3d_expander_data##inst,                                    \
			      &input_fri3d_expander_config##inst, POST_KERNEL,                     \
			      CONFIG_INPUT_FRI3D_EXPANDER_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(INPUT_FRI3D_EXPANDER_DEFINE)
