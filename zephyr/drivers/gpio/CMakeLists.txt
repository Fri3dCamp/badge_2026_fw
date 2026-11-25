# Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
# SPDX-License-Identifier: Apache-2.0

# sources go into Zephyr's drivers__<class> libraries (named relative to zephyr/)
zephyr_library_amend(${ZEPHYR_CURRENT_MODULE_DIR}/zephyr)
zephyr_library_sources_ifdef(CONFIG_GPIO_FRI3D_EXPANDER gpio_fri3d_expander.c)
