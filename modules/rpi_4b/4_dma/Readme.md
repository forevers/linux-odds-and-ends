# dma moc driver

This project provides a canonical dma driver for rpi 4.

Components:
- moc dma driver
    - completion
    - procfs
    - mmap
- dts overlay

## Kernel config requirements
- nothing special

## ess-dma-moc driver
- Build the driver

    - 32 bit target
        ```console
        host$ sudo apt install crossbuild-essential-armhf
        host$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C \<path to cross compiled kernel>/linux M=$(pwd) modules clean
        host$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C \<path to cross compiled kernel>/linux M=$(pwd) modules
        ```

    or

    - 64 bit target

        ```console
        host$ sudo apt install crossbuild-essential-arm64
        host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules clean
        host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules
        ```

- Transfer driver to target for development.
    ```console
    host$ scp ess_dma_moc.ko pi@<pi ip addr>:/dev/shm
    ```

- Manually modify module installation to load module during next boot
    ```console
    pi@raspberrypi:/dev/shm $ sudo cp ess_dma_moc.ko /lib/modules/5.10.52-v7l+/extra
    pi@raspberrypi:/dev/shm $ cd /lib/modules/5.10.52-v7l+/extra
    pi@raspberrypi:/lib/modules/5.10.52-v7l+/extra $ sudo depmod
    ```

- depmod should create this line in modules.dep
    ```console
    pi@raspberrypi:/lib/modules/5.4.77-v8+/extra $ cat ../modules.dep | grep ess_dma_moc
    extra/ess_dma_moc.ko
    ```

## dts overlay
- Compile the dts overlay
    ```console
    host$ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-dma-moc.dtbo ess-dma-moc-overlay.dts
    ```

- Transfer dtbo blob to rpi
    ```console
    host$ scp ess-dma-moc.dtbo pi@<pi ip addr>:/dev/shm
    ```

- Modify owner and permissions
    ```console
    pi@raspberrypi:/dev/shm $ sudo chown root: ess-dma-moc.dtbo
    pi@raspberrypi:/dev/shm $ sudo chmod 755 ess-dma-moc.dtbo
    ```

- Locate blob in overlay directory
    ```console
    pi@raspberrypi:/dev/shm $ sudo cp ess-dma-moc.dtbo /boot/overlays
    ```

- Edit rpi /boot/config.txt such that device-tree overlay applied on next boot
    ```console
    # ess dma moc driver
    dtdebug=1
    dtoverlay=ess-dma-moc
    ```

- Apply the dts overlay immediately
    ```console
    pi@raspberrypi:/dev/shm $ sudo dtoverlay ess-dma-moc.dtbo
   ```

- Verify dts node modifications appear in /proc/device-tree
    ```console
    pi@raspberrypi:~ $ ls /proc/device-tree/soc/ess-dma-moc
    ```

## driver load/unload test

- After driver module interrogate files

- Load, test, unload module with 'dmesg -wH' in separate terminal.
    ```console
    pi@raspberrypi:~ $ ls /dev/ess-dma-moc
    /dev/ess-device-name
    pi@raspberrypi: sudo modprobe ess_dma_moc
    pi@raspberrypi:~ $ cat /proc/ess-proc-name 
    size = 65536
    blaa = ...
    pi@raspberrypi: sudo modprobe ess_dma_moc.ko
    pi@raspberrypi: echo '12345' > /dev/ess-dma-moc
    pi@raspberrypi: sudo modprobe -r ess_dma_moc.ko
    ```
