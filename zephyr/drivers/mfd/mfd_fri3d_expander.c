/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * CH32X035 I/O coprocessor of the Fri3d Camp 2026 badge (badge_2026_fw).
 *
 * With CONFIG_MFD_FRI3D_EXPANDER_UPDATE its firmware can be updated over
 * RVSWD: the coprocessor gives its I2C pins to SWD on request, the
 * zephyr-rvswd module programs it, and the pins go back to I2C.
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

#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE
#include <stdlib.h>

#include <rvswd/ch32.h>
#include <rvswd/rvswd.h>
#endif

LOG_MODULE_REGISTER(mfd_fri3d_expander, CONFIG_MFD_LOG_LEVEL);

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
static void mfd_fri3d_expander_update_inputs(const struct device *dev)
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

/* Wait until the coprocessor answers on I2C, and read its version */
static int mfd_fri3d_expander_wait(const struct device *dev, k_timepoint_t timeout)
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

#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE

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
/* the bundled firmware was installed at boot (for the shell) */
static bool mfd_fri3d_expander_updated_at_boot;

static const uint8_t mfd_fri3d_expander_firmware[] = {
#include "fri3d_expander_firmware.inc"
};

/* Version of the bundled firmware from CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE_VERSION */
static void mfd_fri3d_expander_bundled_version(uint8_t version[3])
{
	const char *p = CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE_VERSION;

	for (int i = 0; i < 3; i++) {
		char *end;

		version[i] = strtoul(p, &end, 10);
		p = *end == '.' ? end + 1 : end;
	}
}

static int mfd_fri3d_expander_update_bundled(const struct device *dev,
					     mfd_fri3d_expander_progress_cb_t cb, void *user_data)
{
	return mfd_fri3d_expander_update(dev, mfd_fri3d_expander_firmware,
					 sizeof(mfd_fri3d_expander_firmware), cb, user_data);
}
#endif /* FRI3D_EXPANDER_HAS_FIRMWARE */

#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE_AT_BOOT
/*
 * At init: install the bundled firmware when the coprocessor runs an older
 * version, or does not answer at all (no firmware). Debug builds report
 * 255.255.255 and are left alone.
 */
static int mfd_fri3d_expander_update_at_boot(const struct device *dev, int answered)
{
#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
	struct mfd_fri3d_expander_data *data = dev->data;
	uint8_t bundled[3];

	mfd_fri3d_expander_bundled_version(bundled);

	if (answered == 0) {
		if (memcmp(data->version, bundled, sizeof(bundled)) >= 0) {
			return 0;
		}
		LOG_INF("%s: firmware v%u.%u.%u is older than v%u.%u.%u, updating", dev->name,
			data->version[0], data->version[1], data->version[2], bundled[0],
			bundled[1], bundled[2]);
	} else {
		LOG_WRN("%s: no answer, installing firmware v%u.%u.%u", dev->name, bundled[0],
			bundled[1], bundled[2]);
	}

	if (mfd_fri3d_expander_update_bundled(dev, NULL, NULL) != 0) {
		return -EIO;
	}
	mfd_fri3d_expander_updated_at_boot = true;

	return 0;
#else
	ARG_UNUSED(dev);

	return answered;
#endif
}
#endif /* CONFIG_MFD_FRI3D_EXPANDER_UPDATE_AT_BOOT */

#endif /* CONFIG_MFD_FRI3D_EXPANDER_UPDATE */

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

#if defined(CONFIG_SHELL) && defined(FRI3D_EXPANDER_HAS_FIRMWARE)
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

static const struct device *const shell_expander = DEVICE_DT_GET(DT_DRV_INST(0));

static void mfd_fri3d_expander_shell_progress(size_t done, size_t total, void *user_data)
{
	const struct shell *sh = user_data;

	/* every 25 % */
	if (done == total || (done * 4 / total) != ((done + 256) * 4 / total)) {
		shell_print(sh, "  %u %%", (unsigned int)(done * 100 / total));
	}
}

static int cmd_expander_version(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t version[3], bundled[3];

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	mfd_fri3d_expander_get_version(shell_expander, version);
	mfd_fri3d_expander_bundled_version(bundled);
	shell_print(sh, "coprocessor firmware v%u.%u.%u, bundled v%u.%u.%u (%u bytes)%s", version[0],
		    version[1], version[2], bundled[0], bundled[1], bundled[2],
		    (unsigned int)sizeof(mfd_fri3d_expander_firmware),
		    mfd_fri3d_expander_updated_at_boot ? ", installed at boot" : "");

	return 0;
}

static int cmd_expander_update(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "installing the bundled coprocessor firmware");
	ret = mfd_fri3d_expander_update_bundled(shell_expander, mfd_fri3d_expander_shell_progress,
						(void *)sh);
	if (ret != 0) {
		shell_error(sh, "update failed: %d", ret);
		return ret;
	}

	cmd_expander_version(sh, 0, NULL);

#ifdef CONFIG_REBOOT
	/* the new firmware reset the display, touch controller and LoRa module */
	shell_print(sh, "restarting");
	k_msleep(100);
	sys_reboot(SYS_REBOOT_COLD);
#else
	shell_print(sh, "restart the badge: the display was reset");
#endif

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_expander,
	SHELL_CMD(version, NULL, "Coprocessor and bundled firmware versions",
		  cmd_expander_version),
	SHELL_CMD(update, NULL, "Install the bundled coprocessor firmware", cmd_expander_update),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(expander, &sub_expander, "Fri3d badge 2026 coprocessor", NULL);
#endif
