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
from machine import I2C, Pin
import struct
import time

ADDRESS = 0x38

def callback(p):
    # read the button states
    print("button state:", expander_i2c.readfrom_mem(ADDRESS, 4, 2).hex())

pin_interrupt = Pin(38, Pin.IN)
pin_interrupt.irq(trigger=Pin.IRQ_RISING, handler=callback)

expander_i2c = I2C(sda=Pin(39), scl=Pin(42), freq=400000)
# read the version:
print("version:", expander_i2c.readfrom_mem(ADDRESS, 0, 3).hex())

# read the analog state
print("analog channels:", struct.unpack("<HHHHHH", expander_i2c.readfrom_mem(ADDRESS, 6, 12)))

# read the debug led state
print("debug LED PWM:", struct.unpack("<H", expander_i2c.readfrom_mem(ADDRESS, 20, 2)))
print("LCD backlight PWM:", struct.unpack("<H", expander_i2c.readfrom_mem(ADDRESS, 22, 2)))

# set the LCD brightness to 50%
expander_i2c.writeto_mem(ADDRESS, 18, struct.pack("<H", 50))

# read the LCD brightness
print("LCD backlight PWM:", struct.unpack("<H", expander_i2c.readfrom_mem(ADDRESS, 18, 2)))

# fade the debug LED up and down
for i in range (100):
    expander_i2c.writeto_mem(ADDRESS, 20, struct.pack("<H", i))
    time.sleep(.1)
for i in range (100, 0, -1):
    expander_i2c.writeto_mem(ADDRESS, 20, struct.pack("<H", i))
    time.sleep(.1)

# turn off 3v3 aux
expander_i2c.writeto_mem(ADDRESS, 22, b'\x00')

# turn on 3v3 aux
expander_i2c.writeto_mem(ADDRESS, 22, b'\x01')

# trigger a reboot to bootloader
expander_i2c.writeto_mem(ADDRESS, 22, b'\x04')

```

### Arduino

```
TODO
```
