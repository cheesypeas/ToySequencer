#!/bin/bash

avrdude -p m16u2 -F -P usb -c usbasp -U flash:w:lib/hiduino/compiled_firmwares/arduino_midi.hex \
-U lfuse:w:0xFF:m -U hfuse:w:0xD9:m -U efuse:w:0xF4:m -U lock:w:0x0F:m

