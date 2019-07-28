# rpi Canonical LKM

Generic LKM for rpi IO.


# Kernel Build

## rpi References
https://www.raspberrypi.org/documentation/linux/kernel/building.md
https://www.raspberrypi.org/documentation/linux/kernel/configuring.md
https://www.raspberrypi.org/documentation/installation/installing-images/README.md

# Build Steps
- Install Raspbian latest prebuilt onto SD card (etcher, ...)
- Determine kernel build version
    - $ uname -r
- Install toolchain for kernel cross compilation
    - $ git clone https://github.com/raspberrypi/tools <path to tools>
    - update path
    - $ echo PATH=\$PATH:<path to tools>/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin >> ~/.bashrc
- Clone kernel source
    - $ git clone --depth=1 --branch rpi-4.19.y https://github.com/raspberrypi/linux
- Kernel Cross Compilation
    - Enter the following commands to build the sources and Device Tree files:
    - $ cd linux
    - $ KERNEL=kernel7
    - $ make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- bcm2709_defconfig
    - to edit kernel config ...
        - $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- menuconfig
    - build images
        - $ make -j4 ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage modules dtbs

# Flash Kernel onto SD card
- 2 options for kernel programming 1) copying over old kernel after backing up or 2) edit the config.txt file to select the kernel that the Pi will boot into
- I'll go with 1)
- Insert SD card and issue $ lsblk do determine target blocks
- $ lsblk
mmcblk0     179:0    0  14.9G  0 disk 
├─mmcblk0p1 179:1    0  42.9M  0 part < media path >/boot
└─mmcblk0p2 179:2    0  14.8G  0 part < media path >/rootfs
- install modules to rootfs
$ sudo make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- INSTALL_MOD_PATH=<media path>/rootfs modules_install
- copy kernel and dtbs
    - if existing sd card … kernel backup
    - $ sudo cp  < media path >/boot/$KERNEL.img  < media path >/boot/$KERNEL-backup.img
    - $ sudo cp arch/arm/boot/zImage  < media path >/boot/$KERNEL.img
    - $ sudo cp arch/arm/boot/dts/*.dtb  < media path >/boot/
    - $ sudo cp arch/arm/boot/dts/overlays/*.dtb*  < media path >/boot/overlays/
    - $ sudo cp arch/arm/boot/dts/overlays/README  < media path >/boot/overlays/
    - $ sudo umount  < media path >/boot
    - $ sudo umount  < media path>/rootfs

# LKM module build
## cross compile
- simple Makefile for module construction
    - $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/steve/projects/rpi/kernel_builds/linux M=$(pwd) modules
- inspect resultant module
    - $ /home/steve/projects/rpi/rpi_tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-objdump ess_canonical.ko -d -j .modinfo
 vermagic=4.19.58-v7+
- scp ko to target and test 
    - sudo insmod <module name>.ko
    - sudo rmmod <module name>
    - dmesg

# target configuration
- SSH between host and target
    - $ ifconfig // note target inet address
    - $ ping <inet address> // from host
    - need to explicity enable ssh on pi
        - $ sudo raspi-config / Interface Options / SSH / enable
    - ssh from host to rpi
        - $ ssh pi@<inet address> // prompted for host pwd
- SCP from host and target
    - Copy file from local host to a remote host SCP example:
        -$ scp < module name >.ko pi@< ip addr >:/home/pi/<target dir>

## Supported Devices

- rpi 3 B v1.2
