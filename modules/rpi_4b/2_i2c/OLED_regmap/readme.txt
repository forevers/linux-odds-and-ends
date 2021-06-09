// TODO pinmux init all GPIO pins
sudo python ~/projects/oled/examples/ssd1306_bonnet_buttons.py prime the rocker gpio configuration
// TODO x-compile app environment, ... required target headers/libraries on host
export TOOL_PREFIX=/mnt/data/projects/rpi/clones/tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi
export CXX=$TOOL_PREFIX-g++
export AR=$TOOL_PREFIX-ar
export RANLIB=$TOOL_PREFIX-ranlib
export CC=$TOOL_PREFIX-gcc
export LD=$TOOL_PREFIX-ld
export CCFLAGS="-march=armv4"
... get libraries/headers
// TODO
- locate dts overlay in directory to allow includes
- kernel build with REGMAP or REGMAP_I2C enabled