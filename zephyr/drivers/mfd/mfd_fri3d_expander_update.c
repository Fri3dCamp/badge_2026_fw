/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Firmware update of the Fri3d Camp 2026 badge's coprocessor over RVSWD
 * (CONFIG_MFD_FRI3D_EXPANDER_UPDATE): the coprocessor gives its I2C pins to
 * SWD on request, the zephyr-rvswd module programs it, and the pins go back
 * to I2C. Also the bundled image.
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander

#include <stdlib.h>
#include <string.h>

#include <rvswd/ch32.h>
#include <rvswd/rvswd.h>
#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/logging/log.h>

#include "mfd_fri3d_expander_priv.h"

LOG_MODULE_DECLARE(mfd_fri3d_expander, CONFIG_MFD_LOG_LEVEL);

/* successful updates since boot (for the shell) */
static unsigned int mfd_fri3d_expander_updates;

/* Delay for the coprocessor to give its I2C pins to SWD after the remap command */
#define REMAP_DELAY_MS 200
/* After the reset into the new firmware: I2C starts after one second */
#define RESTART_TIMEOUT_MS 4000

/* CH32X035 registers to keep PB6 (AUX 3V3 enable) high while the core is halted */
#define CH32_RCC_APB2PCENR 0x40021018
#define CH32_RCC_IOPBEN    BIT(3)
#define CH32_GPIOB_CFGLR   0x40010C00
#define CH32_GPIOB_BSHR    0x40010C10
#define CH32_AUX_PIN       6

/*
 * The core is halted straight out of reset, with PB6 a floating input (the
 * LDO enable has a pull-up). Drive it high anyway, so the AUX rail and the
 * peripherals on it (display, touch, IMU) stay powered while programming.
 */
static int mfd_fri3d_expander_keep_aux_on(const struct device *rvswd)
{
	uint32_t value;
	int ret;

	ret = ch32_read_word(rvswd, CH32_RCC_APB2PCENR, &value);
	if (ret == 0) {
		ret = ch32_write_word(rvswd, CH32_RCC_APB2PCENR, value | CH32_RCC_IOPBEN);
	}
	if (ret == 0) {
		/* output level first, then push-pull output (MODE 11, CNF 00) */
		ret = ch32_write_word(rvswd, CH32_GPIOB_BSHR, BIT(CH32_AUX_PIN));
	}
	if (ret == 0) {
		ret = ch32_read_word(rvswd, CH32_GPIOB_CFGLR, &value);
	}
	if (ret == 0) {
		value &= ~(0xfU << (4 * CH32_AUX_PIN));
		ret = ch32_write_word(rvswd, CH32_GPIOB_CFGLR, value | (0x3U << (4 * CH32_AUX_PIN)));
	}

	return ret;
}

static int mfd_fri3d_expander_program(const struct device *dev, const void *image, size_t len,
				      mfd_fri3d_expander_progress_cb_t cb, void *user_data)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	uint32_t vendor[4];
	const char *chip;
	int ret;

	ret = rvswd_connect(config->rvswd);
	if (ret == 0) {
		/* halts it before it runs any code */
		ret = ch32_identify(config->rvswd, vendor);
	}
	if (ret != 0) {
		LOG_ERR("%s: no answer on SWD: %d", dev->name, ret);
		return ret;
	}

	chip = ch32_chip_name(vendor);
	if (chip == NULL || strncmp(chip, "CH32X03", 7) != 0) {
		LOG_ERR("%s: not a CH32X03x (vendor bytes %08x %08x)", dev->name, vendor[0],
			vendor[1]);
		ch32_reset_and_run(config->rvswd);
		return -ENODEV;
	}

	ret = mfd_fri3d_expander_keep_aux_on(config->rvswd);
	if (ret == 0) {
		LOG_INF("%s: programming %s (%zu bytes)", dev->name, chip, len);
		/* ends with a reset into the new firmware */
		ret = ch32x03x_program(config->rvswd, image, len, cb, user_data);
	}
	if (ret != 0) {
		LOG_ERR("%s: programming failed: %d", dev->name, ret);
		ch32_reset_and_run(config->rvswd);
	}

	return ret;
}

int mfd_fri3d_expander_update(const struct device *dev, const void *image, size_t len,
			      mfd_fri3d_expander_progress_cb_t cb, void *user_data)
{
	const struct mfd_fri3d_expander_config *config = dev->config;
	struct mfd_fri3d_expander_data *data = dev->data;
	int ret, restart;

	if (config->rvswd == NULL) {
		return -ENOTSUP;
	}
	if (!device_is_ready(config->rvswd)) {
		return -ENODEV;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	data->updating = true;

	/*
	 * The coprocessor firmware gives its I2C pins to SWD. Without firmware
	 * it does not answer, and the pins are SWD anyway.
	 */
	i2c_reg_write_byte_dt(&config->i2c, FRI3D_EXPANDER_REG_OUTPUTS,
			      data->outputs | FRI3D_EXPANDER_OUT_REMAP_SWD);
	k_msleep(REMAP_DELAY_MS);

	ret = mfd_fri3d_expander_program(dev, image, len, cb, user_data);
	if (ret == 0) {
		mfd_fri3d_expander_updates++;
	}
	/* the pins go back to I2C */
	rvswd_disconnect(config->rvswd);

	restart = mfd_fri3d_expander_wait(dev, sys_timepoint_calc(K_MSEC(RESTART_TIMEOUT_MS)));
	if (restart == 0) {
		/* the firmware starts with its defaults: restore the outputs */
		data->outputs &= ~(FRI3D_EXPANDER_OUT_REMAP_SWD | FRI3D_EXPANDER_OUT_REBOOT);
		if (data->outputs_valid) {
			restart = i2c_reg_write_byte_dt(&config->i2c, FRI3D_EXPANDER_REG_OUTPUTS,
							data->outputs);
		} else {
			restart = mfd_fri3d_expander_reg_read(dev, FRI3D_EXPANDER_REG_OUTPUTS,
							      &data->outputs, 1);
			data->outputs_valid = restart == 0;
		}
		LOG_INF("%s: firmware v%u.%u.%u", dev->name, data->version[0], data->version[1],
			data->version[2]);
	} else {
		LOG_ERR("%s: no answer after the update", dev->name);
	}

	data->updating = false;
	k_mutex_unlock(&data->lock);

	/* inputs that changed meanwhile */
	mfd_fri3d_expander_update_inputs(dev);

	return ret != 0 ? ret : restart;
}

#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
static const uint8_t mfd_fri3d_expander_firmware[] = {
#include "fri3d_expander_firmware.inc"
};
#endif /* FRI3D_EXPANDER_HAS_FIRMWARE */

int mfd_fri3d_expander_get_bundled_version(uint8_t version[3])
{
#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
	const char *p = CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE_VERSION;

	for (int i = 0; i < 3; i++) {
		char *end;

		version[i] = strtoul(p, &end, 10);
		p = *end == '.' ? end + 1 : end;
	}

	return 0;
#else
	ARG_UNUSED(version);

	return -ENOENT;
#endif
}

int mfd_fri3d_expander_update_bundled(const struct device *dev,
				      mfd_fri3d_expander_progress_cb_t cb, void *user_data)
{
#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
	return mfd_fri3d_expander_update(dev, mfd_fri3d_expander_firmware,
					 sizeof(mfd_fri3d_expander_firmware), cb, user_data);
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(user_data);

	return -ENOENT;
#endif
}

void mfd_fri3d_expander_get_update_info(struct mfd_fri3d_expander_update_info *info)
{
	*info = (struct mfd_fri3d_expander_update_info){
		.updates = mfd_fri3d_expander_updates,
	};
#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
	info->bundled_len = sizeof(mfd_fri3d_expander_firmware);
#endif
}
