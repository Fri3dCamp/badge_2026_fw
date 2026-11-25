/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shell commands of the Fri3d Camp 2026 badge's coprocessor
 * (CONFIG_MFD_FRI3D_EXPANDER_SHELL): "expander version", and with a bundled
 * firmware image "expander update".
 */

#define DT_DRV_COMPAT fri3d_badge_2026_expander

#include <zephyr/drivers/mfd/fri3d_expander.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

#include "mfd_fri3d_expander_priv.h"

static const struct device *const shell_expander = DEVICE_DT_GET(DT_DRV_INST(0));

static int shell_expander_ready(const struct shell *sh)
{
	if (!device_is_ready(shell_expander)) {
		shell_error(sh, "%s not ready", shell_expander->name);
		return -ENODEV;
	}

	return 0;
}

static int cmd_expander_version(const struct shell *sh, size_t argc, char **argv)
{
	uint8_t version[3];
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = shell_expander_ready(sh);
	if (ret != 0) {
		return ret;
	}

	mfd_fri3d_expander_get_version(shell_expander, version);
	shell_print(sh, "coprocessor firmware v%u.%u.%u", version[0], version[1], version[2]);

#ifdef CONFIG_MFD_FRI3D_EXPANDER_UPDATE
	struct mfd_fri3d_expander_update_info info;

	mfd_fri3d_expander_get_update_info(&info);
	if (info.bundled_len > 0) {
		shell_print(sh, "bundled v%u.%u.%u (%zu bytes)%s", info.bundled_version[0],
			    info.bundled_version[1], info.bundled_version[2], info.bundled_len,
			    info.updated_at_boot ? ", installed at boot" : "");
	}
	shell_print(sh, "updates since boot: %u", info.updates);
#endif

	return 0;
}

#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
static void shell_expander_progress(size_t done, size_t total, void *user_data)
{
	const struct shell *sh = user_data;

	/* every 25 % */
	if (done == total || (done * 4 / total) != ((done + 256) * 4 / total)) {
		shell_print(sh, "  %u %%", (unsigned int)(done * 100 / total));
	}
}

static int cmd_expander_update(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = shell_expander_ready(sh);
	if (ret != 0) {
		return ret;
	}

	shell_print(sh, "installing the bundled coprocessor firmware");
	ret = mfd_fri3d_expander_update_bundled(shell_expander, shell_expander_progress,
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
#endif /* FRI3D_EXPANDER_HAS_FIRMWARE */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_expander,
	SHELL_CMD(version, NULL, "Coprocessor (and bundled) firmware versions",
		  cmd_expander_version),
#ifdef FRI3D_EXPANDER_HAS_FIRMWARE
	SHELL_CMD(update, NULL, "Install the bundled coprocessor firmware", cmd_expander_update),
#endif
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(expander, &sub_expander, "Fri3d badge 2026 coprocessor", NULL);
