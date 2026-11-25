/*
 * Copyright (c) 2026 Bert Outtier <outtierbert@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_FRI3D_EXPANDER_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_FRI3D_EXPANDER_H_

/**
 * @defgroup mdf_interface_fri3d_expander Fri3d Badge 2026 IO expander MFD
 * @ingroup mfd_interfaces
 *
 * The CH32X035 coprocessor of the Fri3d Camp 2026 badge runs the
 * badge_2026_fw firmware and acts as an I2C slave with a small register map.
 * The child devices (GPIO, input keys, LEDs, battery sensor, charger) use
 * the functions below.
 * @{
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @name Registers (little endian)
 * @{
 */
#define FRI3D_EXPANDER_REG_VERSION        0x00 /**< 3 bytes: major, minor, patch (R) */
#define FRI3D_EXPANDER_REG_INPUT          0x04 /**< 2 bytes: FRI3D_EXPANDER_IN_* bits (R) */
#define FRI3D_EXPANDER_REG_AIN0           0x08 /**< 2 bytes: multimeter input, raw 12 bit ADC (R) */
#define FRI3D_EXPANDER_REG_BATTERY        0x0a /**< 2 bytes: battery voltage, raw 12 bit ADC (R) */
#define FRI3D_EXPANDER_REG_USB_VOLTAGE    0x0c /**< 2 bytes: USB voltage, raw 12 bit ADC (R) */
#define FRI3D_EXPANDER_REG_JOYSTICK_Y     0x0e /**< 2 bytes: joystick Y, raw 12 bit ADC (R) */
#define FRI3D_EXPANDER_REG_JOYSTICK_X     0x10 /**< 2 bytes: joystick X, raw 12 bit ADC (R) */
#define FRI3D_EXPANDER_REG_LCD_BRIGHTNESS 0x12 /**< 2 bytes: LCD backlight 0-100 (R/W) */
#define FRI3D_EXPANDER_REG_LED_BRIGHTNESS 0x14 /**< 2 bytes: debug LED 0-100 (R/W) */
#define FRI3D_EXPANDER_REG_OUTPUTS        0x16 /**< 1 byte: FRI3D_EXPANDER_OUT_* bits (R/W) */
/** @} */

/** @name Input register bits (1 = active)
 * @{
 */
#define FRI3D_EXPANDER_IN_CHARGING    BIT(0)  /**< charger is charging */
#define FRI3D_EXPANDER_IN_CHARGED     BIT(1)  /**< charger standby: charge complete */
#define FRI3D_EXPANDER_IN_BUTTON_X    BIT(2)
#define FRI3D_EXPANDER_IN_BUTTON_Y    BIT(3)
#define FRI3D_EXPANDER_IN_BUTTON_A    BIT(4)
#define FRI3D_EXPANDER_IN_BUTTON_B    BIT(5)
#define FRI3D_EXPANDER_IN_BUTTON_MENU BIT(6)
#define FRI3D_EXPANDER_IN_JOY_UP      BIT(7)
#define FRI3D_EXPANDER_IN_JOY_DOWN    BIT(8)
#define FRI3D_EXPANDER_IN_JOY_LEFT    BIT(9)
#define FRI3D_EXPANDER_IN_JOY_RIGHT   BIT(10)
#define FRI3D_EXPANDER_IN_USB         BIT(11) /**< USB power present */
/** @} */

/** @name Output register bits
 * @{
 */
#define FRI3D_EXPANDER_OUT_AUX_3V3    BIT(0) /**< 1 = AUX 3V3 rail on */
#define FRI3D_EXPANDER_OUT_LCD_RESET  BIT(1) /**< 1 = LCD and touch released from reset */
#define FRI3D_EXPANDER_OUT_REBOOT     BIT(2) /**< write 1: reboot into the WCH bootloader */
#define FRI3D_EXPANDER_OUT_REMAP_SWD  BIT(3) /**< write 1: I2C pins become SWD (for reflashing) */
#define FRI3D_EXPANDER_OUT_LORA_RESET BIT(4) /**< 1 = LoRa module released from reset */
/** @} */

/**
 * @brief Read registers.
 *
 * @param dev Expander MFD device.
 * @param reg First register.
 * @param data Buffer for the register contents.
 * @param len Number of bytes to read.
 * @retval 0 on success, negative errno otherwise.
 */
int mfd_fri3d_expander_reg_read(const struct device *dev, uint8_t reg, void *data, size_t len);

/**
 * @brief Read a 16 bit register.
 */
int mfd_fri3d_expander_reg_read16(const struct device *dev, uint8_t reg, uint16_t *val);

/**
 * @brief Write a 16 bit register (LCD or LED brightness).
 */
int mfd_fri3d_expander_reg_write16(const struct device *dev, uint8_t reg, uint16_t val);

/**
 * @brief Update bits of the output register.
 *
 * The driver keeps a copy of the register, so this is a single I2C write.
 *
 * @param dev Expander MFD device.
 * @param mask Bits to change (FRI3D_EXPANDER_OUT_*).
 * @param val New value of those bits.
 * @retval 0 on success, negative errno otherwise.
 */
int mfd_fri3d_expander_outputs_update(const struct device *dev, uint8_t mask, uint8_t val);

/**
 * @brief Get the last written value of the output register.
 */
uint8_t mfd_fri3d_expander_outputs_get(const struct device *dev);

/**
 * @brief Get the firmware version of the coprocessor, read at init.
 *
 * @param dev Expander MFD device.
 * @param version Buffer for major, minor and patch.
 */
void mfd_fri3d_expander_get_version(const struct device *dev, uint8_t version[3]);

/**
 * @brief Get the input register as last read after an INT pulse (or poll).
 *
 * @return FRI3D_EXPANDER_IN_* bits.
 */
uint16_t mfd_fri3d_expander_get_inputs(const struct device *dev);

/**
 * @brief Number of times the inputs were read after an INT pulse.
 *
 * Useful to check the INT line: it stays 0 when inputs change but the
 * interrupt never fires. Always 0 when polling.
 */
uint32_t mfd_fri3d_expander_get_int_count(const struct device *dev);

/**
 * @brief Add a callback for input changes.
 *
 * The callback is called from the system work queue with the changed
 * FRI3D_EXPANDER_IN_* bits as @p pins, for the bits in its pin mask. Read
 * the new state with mfd_fri3d_expander_get_inputs().
 */
int mfd_fri3d_expander_add_callback(const struct device *dev, struct gpio_callback *callback);

/**
 * @brief Remove a callback added with mfd_fri3d_expander_add_callback().
 */
int mfd_fri3d_expander_remove_callback(const struct device *dev, struct gpio_callback *callback);

/**
 * @brief Firmware update progress.
 *
 * @param done Bytes written.
 * @param total Bytes to write.
 * @param user_data User data passed to mfd_fri3d_expander_update().
 */
typedef void (*mfd_fri3d_expander_progress_cb_t)(size_t done, size_t total, void *user_data);

/**
 * @brief Install new coprocessor firmware over RVSWD.
 *
 * Needs CONFIG_MFD_FRI3D_EXPANDER_UPDATE and the rvswd property. The
 * coprocessor gives its I2C pins to SWD, is programmed (every page is read
 * back) with the AUX rail kept on, and restarts into the new firmware; the
 * pins go back to I2C and the output register is restored. The coprocessor
 * functions (inputs, outputs, LEDs, battery) are unavailable meanwhile. Takes
 * about 7 seconds for 7 kB.
 *
 * The new firmware pulses the LCD and LoRa reset when it starts: the display,
 * touch controller and LoRa module need to be initialized again, e.g. by
 * restarting. At boot (CONFIG_MFD_FRI3D_EXPANDER_UPDATE_AT_BOOT) the update
 * runs before their drivers initialize.
 *
 * @param dev Expander MFD device.
 * @param image Firmware binary (badge_2026_fw release build).
 * @param len Image length.
 * @param cb Progress callback, or NULL.
 * @param user_data Passed to the callback.
 * @retval 0 on success.
 * @retval -ENOTSUP without the rvswd property.
 * @retval -ENODEV if the target is not a CH32X03x.
 * @retval other negative errno on a programming or I2C error.
 */
int mfd_fri3d_expander_update(const struct device *dev, const void *image, size_t len,
			      mfd_fri3d_expander_progress_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_FRI3D_EXPANDER_H_ */
