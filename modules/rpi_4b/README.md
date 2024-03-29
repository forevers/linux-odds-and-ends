# SD card tools

- etcher or Raspberry PI Imager (rpi-imager)
- rpi-update: `update your Raspberry Pi OS kernel and VideoCore firmware to the latest pre-release versions`
    - https://www.raspberrypi.org/documentation/raspbian/applications/rpi-update.md

# ssh configuration

- https://www.raspberrypi.org/documentation/remote-access/ssh/
    ```console
    $ sudo systemctl enable ssh
    $ sudo systemctl start ssh
    ```
- pi account has sudo privileges, 'raspberry' default pwd

# OS versions

- OS versions
    - https://www.zdnet.com/article/what-is-the-raspberry-pi-4-everything-you-need-to-know-about-the-tiny-low-cost-computer/#:~:text=IS%20THE%20RASPBERRY%20PI%204%2064%2DBIT%3F,to%20run%20on%20the%20Pi.
        - IS THE RASPBERRY PI 4 64-BIT?
        - Yes, it's a 64-bit board. However, there are limited benefits to the 64-bit processor, outside of a few more operating systems possibly being able to run on the Pi.
        - Rather than offering a 64-bit version of the official Raspbian operating system, the Raspberry Pi Foundation has said it wants to focus on optimizing the Pi's official Raspbian OS for 32-bit performance to benefit the millions of older, 32-bit Pi boards that have already been sold.
    - https://webtechie.be/post/2020-09-29-64bit-raspbianos-on-raspberrypi4-with-usbboot/
        - Raspbian OS is the operating system provided by Raspberry Pi and is based on Debian. As only the latest Raspberry Pi-boards have a 64-bit chip, the official release of Raspbian OS is 32-bit only. But there is a work-in-progress-version of Raspbian OS which is 64-bit! Let’s use that one…
    - https://www.raspberrypi.org/forums/viewtopic.php?f=117&t=275370
        - raspberrypi.org discussion of 64 bit debian based builds
    - https://www.raspberrypi.org/blog/latest-raspberry-pi-os-update-may-2020/
        - 64 bit still seems like a work in progress

# rpi Imager SD card install

- Install latest Raspberry PI OS 32 bit version available from rpi Imager

- check version
    ```console
    pi@raspberrypi:~ $ cat /proc/version 
    Linux version 5.4.51-v7l+ (dom@buildbot) (gcc version 4.9.3 (crosstool-NG crosstool-ng-1.22.0-88-g8460611)) #1333 SMP Mon Aug 10 16:51:40 BST 2020
    ```
- sudo apt update
- sudo apt full-upgrade
    - full-upgrade performs the function of upgrade but will remove currently installed packages if this is needed to upgrade the system as a whole.
- version after update
    ```console
    pi@raspberrypi:~ $ cat /proc/version 
    Linux version 5.4.72-v7l+ (dom@buildbot) (gcc version 9.3.0 (Ubuntu 9.3.0-17ubuntu1~20.04)) #1356 SMP Thu Oct 22 13:57:51 BST 2020
    ```

# rpi utils
- i2c
    - $ sudo i2cdetect -y 1
- gpio bus layout
    - https://pinout.xyz/pinout/i2c
    - $ pinout

# kernel build

## kernel source

- clone desired kernel source
    ```console
    $ git clone https://github.com/raspberrypi/linux
    ```

- configure kernel
    ```console
    $ cd linux
    $ KERNEL=kernel7l
    $ make bcm2711_defconfig
    ```

## git interrogation

- Makfile git log reveals one sha per release
    ```console
    $ git log --follow Makefile
    commit 52f6ded2a377ac4f191c84182488e454b1386239 (HEAD)
    Author: Greg Kroah-Hartman <gregkh@linuxfoundation.org>
    Date:   Sat Oct 17 10:11:24 2020 +0200

        Linux 5.4.72
        
        Tested-by: Jon Hunter <jonathanh@nvidia.com>
        Tested-by: Guenter Roeck <linux@roeck-us.net>
        Tested-by: Linux Kernel Functional Testing <lkft@linaro.org>
        Link: https://lore.kernel.org/r/20201016090437.308349327@linuxfoundation.org
        Signed-off-by: Greg Kroah-Hartman <gregkh@linuxfoundation.org>
        ...
    ```

- search git hub commits for this sha
    - https://github.com/raspberrypi/linux/commits
    - https://github.com/raspberrypi/linux/commit/52f6ded2a377ac4f191c84182488e454b1386239

## SD card adapter mounting

- determine target blocks
    ```console
    $ lsblk
    NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
    ...
    sdc      8:32   1  14.9G  0 disk 
    ├─sdc1   8:33   1   256M  0 part <media path>/boot
    └─sdc2   8:34   1  14.6G  0 part <media path>/rootfs
    ```

## build the sources and device tree files

- https://www.raspberrypi.org/documentation/linux/kernel/building.md

- 32 bit builds
    - cross compiler install
        ```console
        $ sudo apt install crossbuild-essential-armhf
        ```
    - build kernel config
        ```console
        $ cd linux
        $ make mrproper
        $ KERNEL=kernel7l
        $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcm2711_defconfig
        ```
    - build kernel, kernel modules, and dtb blob
        ```console
        $ make -j$((`nproc`+1)) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage modules dtbs
        ```
    - install modules to rootfs /lib/modules/<kernel version>. sd card requires /boot and .rootfs partitions
        ```console
        host $  mount
        ...
        /dev/sdc1 on /media/<user account>/boot type vfat (rw,nosuid,nodev,relatime,uid=1001,gid=1001,fmask=0022,dmask=0022,codepage=437,iocharset=iso8859-1,shortname=mixed,showexec,utf8,flush,errors=remount-ro,uhelper=udisks2)
        /dev/sdc2 on /media/<user account>/rootfs type ext4 (rw,nosuid,nodev,relatime,data=ordered,uhelper=udisks2)
        
        host $ df -h
        ...
        /dev/sdc1       253M   70M  183M  28% /media/<user account>/boot
        /dev/sdc2        15G  3.2G   11G  24% /media/<user account>/rootfs
        ```

        ```console
        $ MEDIA_PATH=/media/<user account>
        $ sudo env PATH=$PATH make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=$MEDIA_PATH/rootfs modules_install
        ```
    - 2 options for kernel programming 1) copying over old kernel after backing up or 2) edit the config.txt file to select the kernel that the Pi will boot into
        - I'll go with 1) and copy kernel and dtbs
            ```console
            host $ sudo cp $MEDIA_PATH/boot/$KERNEL.img $MEDIA_PATH/boot/$KERNEL-backup.img
            host $ sudo cp arch/arm/boot/zImage $MEDIA_PATH/boot/$KERNEL.img
            host $ sudo cp arch/arm/boot/dts/*.dtb $MEDIA_PATH/boot/
            host $ sudo cp arch/arm/boot/dts/overlays/*.dtb* $MEDIA_PATH/boot/overlays/
            host $ sudo cp arch/arm/boot/dts/overlays/README $MEDIA_PATH/boot/overlays/
            host $ sudo umount $MEDIA_PATH/boot
            host $ sudo umount $MEDIA_PATH/rootfs
            ```
    - version
        ```console
        pi@raspberrypi:~ $ cat /proc/version 
        Linux version 5.4.77-v7l+ (root@c83ee2eb3534) (gcc version 7.5.0 (Ubuntu/Linaro 7.5.0-3ubuntu1~18.04)) #1 SMP Wed Nov 25 19:31:50 UTC 2020
        ```

- 64 bit builds
    - cross compiler install
        ```console
        host $ sudo apt install crossbuild-essential-arm64
        ```
    - build kernel config
        ```console
        host $ cd linux
        host $ make mrproper
        host $ KERNEL=kernel8
        host $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
        ```
    - to graphically modify default kernel config
        ```console
        host $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
        ```
    - build kernel, kernel modules, and dtb blob
        ```console
       host $ make -j$((`nproc`+1)) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs
        ```
    - install modules to rootfs
        ```console
        host $ MEDIA_PATH=<media path to rootfs and boot>
        host $ sudo env PATH=$PATH make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=$MEDIA_PATH/rootfs modules_install
        ```
    - 2 options for kernel programming 1) copying over old kernel after backing up or 2) edit the config.txt file to select the kernel that the Pi will boot into
        - I'll go with 1) and copy kernel and dtbs
            ```console
            host $ sudo cp $MEDIA_PATH/boot/$KERNEL.img $MEDIA_PATH/boot/$KERNEL-backup.img
            host $ sudo cp arch/arm64/boot/Image $MEDIA_PATH/boot/$KERNEL.img
            host $ sudo cp arch/arm64/boot/dts/broadcom/*.dtb $MEDIA_PATH/boot/
            host $ sudo cp arch/arm64/boot/dts/overlays/*.dtb* $MEDIA_PATH/boot/overlays/
            host $ sudo cp arch/arm64/boot/dts/overlays/README $MEDIA_PATH/boot/overlays/
            host $ sudo umount $MEDIA_PATH/boot
            host $ sudo umount $MEDIA_PATH/boot/rootfs
            ```
    - add 'arm_64bit=1' to /boot/config.txt to boot 64bit kernel
        - https://www.raspberrypi.org/documentation/configuration/config-txt/boot.md
    - version
        ```console
        pi@raspberrypi:~ $ cat /proc/version 
        ...
        ```

# TODO
- extend docker image to support LKM and built ins
- rpi utilities for i2c
- rpi utilities for spi

# LKM module build

## cross compile 32 bit
- simple Makefile for module construction
    - $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C \<path to cross compiled kernel>/linux -mabi=ilp32 M=$(pwd) modules
    - $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C \<path to cross compiled kernel>/linux -mabi=ilp32 M=$(pwd) clean
- inspect resultant module
    - $ \<path to rpi_tools>/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-objdump <module name>.ko -d -j .modinfo
 vermagic=4.19.58-v7+

## cross compile 64 bit
- simple Makefile for module construction
    ```console
    obj-m := module_name.o
    module_name-objs := module_1.o <other modules ...>
    ```
- environment
-- using linux tools from kernel clone:
    ```console
        $ export TOOL_PREFIX=<path to rpi kernel clone directory>/linux
        $ export CXX=$TOOL_PREFIX-g++
        $ export AR=$TOOL_PREFIX-ar
        $ export RANLIB=$TOOL_PREFIX-ranlib
        $ export CC=$TOOL_PREFIX-gcc
        $ export LD=$TOOL_PREFIX-ld
        $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /<path to cross compiled kernel>/linux M=$(pwd) clean
        $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /<path to cross compiled kernel>/linux M=$(pwd) modules
    ```
-- using linux tools from https://github.com/raspberrypi/tools
    ```console
        $ export TOOL_PREFIX=<path to rpi tools directory>/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi
        $ export CXX=$TOOL_PREFIX-g++
        $ export AR=$TOOL_PREFIX-ar
        $ export RANLIB=$TOOL_PREFIX-ranlib
        $ export CC=$TOOL_PREFIX-gcc
        $ export LD=$TOOL_PREFIX-ld
        $ export TOOL_PREFIX=<rpi kernel clone directory>/linux
        $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /<path to cross compiled kernel>/linux M=$(pwd) clean
        $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /<path to cross compiled kernel>/linux M=$(pwd) modules
    ```

# transfer x-compiled modules to target
- SSH between host and target
    - $ ifconfig // note target inet address
    - $ ping <inet address> // from host
    - need to explicitly enable ssh on pi
        - $ sudo raspi-config / Interface Options / SSH / enable
    - ssh from host to rpi
        - $ ssh pi@<inet address> // prompted for host pwd
- SCP from host and target
    - Copy file from local host to a remote host SCP example:
        -$ scp ess_oled.ko.ko pi@<ip addr>:/home/pi/projects/oled/driver
    
# test
- load, test, unload module
    - sudo insmod \<module name>.o
    - dmesg -wH in separate shell
    - execute test
    - sudo rmmod \<module name>

# cross compile app

## rpi tools repo

https://github.com/raspberrypi/tools
TOOL_PREFIX points to x-compile binary tools
environment configuration

    ```console
    export TOOL_PREFIX=/mnt/data/projects/rpi/clones/tools/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/bin/arm-bcm2708hardfp-linux-gnueabi
    export CXX=$TOOL_PREFIX-g++
    export AR=$TOOL_PREFIX-ar
    export RANLIB=$TOOL_PREFIX-ranlib
    export CC=$TOOL_PREFIX-gcc
    export LD=$TOOL_PREFIX-ld
    export CCFLAGS="-march=armv4"
    ```

TODO: rfs portion of host xcompile env ... like https://github.com/ionutneicu/rpi-cross-scripts


## Supported Devices
- rpi 4

## rpi references

https://www.raspberrypi.org/documentation/linux/kernel/building.md
https://www.raspberrypi.org/documentation/linux/kernel/configuring.md
https://www.raspberrypi.org/documentation/installation/installing-images/README.md

# i2c driver references
https://www.kernel.org/doc/html/latest/i2c/writing-clients.html