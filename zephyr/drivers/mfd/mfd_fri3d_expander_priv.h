/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shared by the coprocessor's MFD driver (mfd_fri3d_expander.c) and its
 * firmware update over RVSWD (mfd_fri3d_expander_update.c).
 */

#ifndef ZEPHYR_DRIVERS_MFD_MFD_FRI3D_EXPANDER_PRIV_H_
#define ZEPHYR_DRIVERS_MFD_MFD_FRI3D_EXPANDER_PRIV_H_

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

struct mfd_fri3d_expander_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec int_gpio;
	uint32_t boot_timeout_ms;
#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE
	/* RVSWD interface on the coprocessor's I2C pins, or NULL */
	const struct device *rvswd;
#endif
};

struct mfd_fri3d_expander_data {
	const struct device *dev;
	struct k_mutex lock;
	uint8_t version[3];
	uint8_t outputs;
	/* the output register cache holds what the coprocessor has */
	bool outputs_valid;
	/* a firmware update is running: the coprocessor does not answer on I2C */
	bool updating;
	uint16_t inputs;
	uint32_t int_count;
	sys_slist_t callbacks;
#ifdef CONFIG_MFD_FRI3D_EXPANDER_POLL
	struct k_work_delayable poll_work;
#else
	struct k_work int_work;
	struct gpio_callback int_cb;
#endif
};

/* Wait until the coprocessor answers on I2C, and read its version */
int mfd_fri3d_expander_wait(const struct device *dev, k_timepoint_t timeout);

/* Read the inputs and call the callbacks of the ones that changed */
void mfd_fri3d_expander_update_inputs(const struct device *dev);

/* What the update code holds, for the shell (CONFIG_MFD_FRI3D_EXPANDER_UPDATE) */
struct mfd_fri3d_expander_update_info {
	/* bundled image (CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE): size, 0 without one */
	size_t bundled_len;
	/* successful updates since boot */
	unsigned int updates;
};

void mfd_fri3d_expander_get_update_info(struct mfd_fri3d_expander_update_info *info);

#endif /* ZEPHYR_DRIVERS_MFD_MFD_FRI3D_EXPANDER_PRIV_H_ */
