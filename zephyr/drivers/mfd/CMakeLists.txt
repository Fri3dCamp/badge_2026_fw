# Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
# SPDX-License-Identifier: Apache-2.0

# sources go into Zephyr's drivers__mfd library (named relative to zephyr/)
zephyr_library_amend(${ZEPHYR_CURRENT_MODULE_DIR}/zephyr)
zephyr_library_sources_ifdef(CONFIG_MFD_FRI3D_EXPANDER mfd_fri3d_expander.c)

# The update uses the zephyr-rvswd module (west project, module name rvswd)
if(CONFIG_MFD_FRI3D_EXPANDER_UPDATE AND NOT CONFIG_RVSWD_CH32X03X)
  if(NOT ZEPHYR_RVSWD_MODULE_DIR)
    message(FATAL_ERROR "CONFIG_MFD_FRI3D_EXPANDER_UPDATE needs the zephyr-rvswd module: "
            "add github.com/bertouttier/zephyr-rvswd to the west manifest")
  endif()
  message(FATAL_ERROR "CONFIG_MFD_FRI3D_EXPANDER_UPDATE needs an enabled wch,rvswd node "
          "as the expander's rvswd property")
endif()

# Coprocessor firmware image to link in (CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE)
if(CONFIG_MFD_FRI3D_EXPANDER_UPDATE AND NOT CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE STREQUAL "")
  get_filename_component(fri3d_expander_fw ${CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE} ABSOLUTE
                         BASE_DIR ${APPLICATION_SOURCE_DIR})
  if(NOT EXISTS ${fri3d_expander_fw})
    message(FATAL_ERROR "CONFIG_MFD_FRI3D_EXPANDER_FIRMWARE: ${fri3d_expander_fw} not found")
  endif()
  generate_inc_file_for_target(${ZEPHYR_CURRENT_LIBRARY} ${fri3d_expander_fw}
                               ${ZEPHYR_BINARY_DIR}/include/generated/fri3d_expander_firmware.inc)
  zephyr_library_compile_definitions(FRI3D_EXPANDER_HAS_FIRMWARE)
endif()
