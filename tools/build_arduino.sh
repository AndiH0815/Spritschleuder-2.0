#!/bin/sh
# Compile-only reference target: actual board and active pin levels must be checked.
set -eu
cd "$(dirname "$0")/.."
: "${AVR_TOOLCHAIN:?Set AVR_TOOLCHAIN to the avr-gcc installation directory}"
: "${ARDUINO_AVR_CORE:?Set ARDUINO_AVR_CORE to ArduinoCore-avr root}"
out=${1:-tests/build/uno}
mkdir -p "$out"
core="$ARDUINO_AVR_CORE/cores/arduino"
variant="$ARDUINO_AVR_CORE/variants/standard"
for source in "$core"/*.c "$core"/*.cpp main/arduino_main.cpp; do
  case "$source" in *.cpp) compiler="$AVR_TOOLCHAIN/bin/avr-g++"; extra="-std=gnu++11 -fno-exceptions -fno-threadsafe-statics";; *) compiler="$AVR_TOOLCHAIN/bin/avr-gcc"; extra="-std=gnu11";; esac
  "$compiler" -Os -Wall -Wextra -ffunction-sections -fdata-sections -flto -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR $extra -I"$core" -I"$variant" -c "$source" -o "$out/$(basename "$source").o"
done
"$AVR_TOOLCHAIN/bin/avr-gcc" -Os -flto -mmcu=atmega328p -Wl,--gc-sections -o "$out/pump-controller.elf" "$out"/*.o -lm
"$AVR_TOOLCHAIN/bin/avr-objcopy" -O ihex -R .eeprom "$out/pump-controller.elf" "$out/pump-controller.hex"
"$AVR_TOOLCHAIN/bin/avr-size" -C --mcu=atmega328p "$out/pump-controller.elf"
