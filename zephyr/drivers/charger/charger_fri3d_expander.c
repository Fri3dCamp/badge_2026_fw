/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * TP4056 charger status of the Fri3d Camp 2026 badge, as reported by the I/O
 * coprocessor (read-only).
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander_charger

#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(charger_fri3d_expander, CONFIG_CHARGER_LOG_LEVEL);

struct charger_fri3d_expander_config {
	const struct device *mfd;
};

static int charger_fri3d_expander_get_prop(const struct device *dev, const charger_prop_t prop,
					   union charger_propval *val)
{
	const struct charger_fri3d_expander_config *config = dev->config;
	uint16_t inputs;
	bool usb;
	int ret;

	ret = mfd_fri3d_expander_reg_read16(config->mfd, FRI3D_EXPANDER_REG_INPUT, &inputs);
	if (ret != 0) {
		return ret;
	}

	usb = (inputs & FRI3D_EXPANDER_IN_USB) != 0;

	switch (prop) {
	case CHARGER_PROP_ONLINE:
		val->online = usb ? CHARGER_ONLINE_FIXED : CHARGER_ONLINE_OFFLINE;
		return 0;
	case CHARGER_PROP_STATUS:
		/* TP4056: CHRG low while charging, STDBY low when the charge is complete */
		if (inputs & FRI3D_EXPANDER_IN_CHARGING) {
			val->status = CHARGER_STATUS_CHARGING;
		} else if (inputs & FRI3D_EXPANDER_IN_CHARGED) {
			val->status = CHARGER_STATUS_FULL;
		} else if (usb) {
			/* e.g. no battery connected */
			val->status = CHARGER_STATUS_NOT_CHARGING;
		} else {
			val->status = CHARGER_STATUS_DISCHARGING;
		}
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int charger_fri3d_expander_set_prop(const struct device *dev, const charger_prop_t prop,
					   const union charger_propval *val)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(prop);
	ARG_UNUSED(val);

	/* the TP4056 has no control interface */
	return -ENOTSUP;
}

static int charger_fri3d_expander_charge_enable(const struct device *dev, const bool enable)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(enable);

	return -ENOTSUP;
}

static DEVICE_API(charger, charger_fri3d_expander_api) = {
	.get_property = charger_fri3d_expander_get_prop,
	.set_property = charger_fri3d_expander_set_prop,
	.charge_enable = charger_fri3d_expander_charge_enable,
};

static int charger_fri3d_expander_init(const struct device *dev)
{
	const struct charger_fri3d_expander_config *config = dev->config;

	if (!device_is_ready(config->mfd)) {
		LOG_ERR("%s: expander not ready", dev->name);
		return -ENODEV;
	}

	return 0;
}

#define CHARGER_FRI3D_EXPANDER_DEFINE(inst)                                                        \
	static const struct charger_fri3d_expander_config charger_fri3d_expander_config##inst = {  \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, charger_fri3d_expander_init, NULL, NULL,                       \
			      &charger_fri3d_expander_config##inst, POST_KERNEL,                   \
			      CONFIG_CHARGER_INIT_PRIORITY, &charger_fri3d_expander_api);

DT_INST_FOREACH_STATUS_OKAY(CHARGER_FRI3D_EXPANDER_DEFINE)
