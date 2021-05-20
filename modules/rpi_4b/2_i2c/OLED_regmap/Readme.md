# I2C Regmap/GPIO OLED driver and Test Application

This project provides a fundamental rpi 4 display driver for an adafruit 128x64 oled bonnet.

Components:
- Solomon Systech OLED display
    - i2c based (OLED controller also supports spi)
    - gpios and rocker switch
- dts overlay
- i2c Regmap driver
- GPIO irq bottom half work queue processing
- user space test app supports ioctl command set


## driver
Build the driver
```console
host$ sudo apt install crossbuild-essential-arm64
host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules clean
host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules
```

Transfer driver to target
```console
host$ scp ess_oled_regmap.ko pi@<pi ip addr>:/home/pi/projects/oled/driver
```

## dts overlay
Build the dts overlay
```console
host$ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-i2c-oled.dtbo ess-i2c-oled-overlay.dts
```

Transfer dtbo into rpi /boot/overlays directory
```console
host$ scp ess-i2c-oled.dtbo pi@xxx.xxx.xxx.xxx:/home/pi
```

Modify owner and permissions
```console
pi@raspberrypi:~ sudo chown root: ess-i2c-oled.dtbo
pi@raspberrypi:~ sudo chmod 755 ess-i2c-oled.dtbo
pi@raspberrypi:~ sudo mv ess-i2c-oled.dtbo /boot/overlays
```

Edit rpi /boot/config.txt
```console
# ess oled bonnet driver
dtdebug=1
dtoverlay=ess-i2c-oled
```

## driver test app

Build test app on target
```console
pi@raspberrypi: g++ -g -O0 -std=c++17 -ggdb -lpthread -o i2c_oled_test main.cpp
```

Load, test, unload module
```console
pi@raspberrypi: dmesg -wH in separate shell
pi@raspberrypi: sudo insmod ess_oled_regmap.ko
pi@raspberrypi: i2c_oled_test
pi@raspberrypi: sudo rmmod ess_oled_regmap.ko
```

### detailed docs
[I2CDev on rpi 4](I2CDEV_ENABLE.md): Enabling i2cdev on rpi 4

[OLED Device Info](OLED_DEVICE_INFO.md): Information on the Solomon Systech OLED and Python tools targeting the OLED

[Device Tree overlay on rpi 4](DEVICE_TREE_OVERLAY.md): Details on creating and installing a driver dts overlay