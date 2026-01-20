## Fri3d Badge 2026 IO expander firmware

This firmware runs on the [CH32X035](https://www.wch-ic.com/products/CH32X035.html) companian MCU of the [Fri3d Camp](https://fri3d.be) 2026 badge.

The firmware handles different functions of the badge and acts as a I2C IO expander to the main ESP32-S3 MCU of the badge:
 * Battery and charger monitoring
 * USB power monitoring
 * Joystick and button handling
 * LCD brightness and reset control
 * debug LED control
 * Buzzer control

Please check the [schematics](local_link) to get more details about how these peripherals of the badge are connected to the CH32X035 MCU.

The expander has I2C address `0x38` and uses the following registers to interface/control with its connected peripherals:

| Address | Name | Read/Write | Bytes | comments |
|-|-|-|-|-|
| 0x00 | Battery monitor | R | 1 | Reports the battery voltage |
| 0x01 | USB monitor | R | 1 | Reports the USB voltage |
| 0x02 | Joystick X | R | 2 | Reports the current joystick position on the X axis (0-4096) |
| 0x04 | Joystick Y | R | 2 | Reports the current joystick position on the Y axis (0-4096) |
| 0x06 | Digital inputs | R | 2 | description of bits |
| 0x08 | Digital outputs | R/W | 1 | description of bits |
| 0x09 | Debug LED brightness | R/W | 1 | Set the brightness of the debug LED (0-255) |
| 0x0A | LCD backlight brightness | R/W | 1 | Set the brightness of the LCD screen (0-255) |


## Building

Use [platformio](https://platformio.org) to build this project. If you use the command line, build using:

```
pio run
```

## Flashing

TODO

## Development

### Micropython
In Micropython on the ESP32-S3, use the `readfrom_mem()` and `writeto_mem()` I2C APIs to interface with the expander, e.g.:

```
from machine import I2C

ADDRESS = 0x38

i2c = I2C(freq=400000)          # create I2C peripheral at frequency of 400kHz
                                # depending on the port, extra parameters may be required
                                # to select the peripheral and/or pins to use

i2c.scan()                      # scan for peripherals, returning a list of 7-bit addresses

i2c.readfrom_mem(ADDRESS, 8, 3)      # read 3 bytes from memory of peripheral ADDRESS,
                                     #   starting at memory-address 8 in the peripheral
i2c.writeto_mem(ADDRESS, 2, b'\x10') # write 1 byte to memory of peripheral ADDRESS
                                     #   starting at address 2 in the peripheral

```

### Arduino

```
TODO
```
