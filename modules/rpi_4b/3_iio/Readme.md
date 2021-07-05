# IIO moc driver and Test Application

This project provides a canonical iio driver for rpi 4.

Components:
- moc iio driver
- dts overlay
- user space test app supports ioctl command set

## Kernel config requirements
- CONFIG_IIO_SYSFS_TRIGGER=m
    - sudo modprobe iio-trig-sysfs
- analog devices libiio utilities: 
    - https://swdownloads.analog.com/cse/travis_builds/master_latest_libiio-raspbian-9-armhf.deb
    - https://github.com/analogdevicesinc/libiio/releases/tag/v0.21

## ess-iio-moc driver
- Build the driver
    ```console
    host$ sudo apt install crossbuild-essential-arm64
    host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules clean
    host$ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C <path to built kernel>/linux M=$(pwd) modules
    ```

- Transfer driver to target for development.
    ```console
    host$ scp ess_oled.ko pi@<pi ip addr>:/home/pi/projects/drivers
    ```

## dts overlay
- Compile the dts overlay
    ```console
    host$ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-iio-moc.dtbo ess-iio-moc-overlay.dts
    ```

- Transfer dtbo blob to rpi
    ```console
    host$ scp ess-iio-moc.dtbo pi@xxx.xxx.xxx.xxx:/dev/shm
    ```

- Modify owner and permissions
    ```console
    pi@raspberrypi:/dev/shm $ sudo chown root: ess-iio-moc.dtbo
    pi@raspberrypi:/dev/shm $ sudo chmod 755 ess-iio-moc.dtbo
    ```

- Locate blob in overlay directory
    ```console
    pi@raspberrypi:/dev/shm $ sudo cp ess-iio-moc.dtbo /boot/overlays
    ```

- Apply the dts overlay immediately
    ```console
    pi@raspberrypi:/dev/shm $ sudo dtoverlay ess-iio-moc.dtbo
   ```

- Verify dts node modifications appear in /proc/device-tree
    ```console
    pi@raspberrypi:~ $ ls /proc/device-tree/soc/ess-iio-moc
    ```

- Manually modify module installation to load module during next boot
    ```console
    pi@raspberrypi:/dev/shm $ sudo cp ess-iio-moc.ko /lib/modules/5.4.77.-v8+/extra
    pi@raspberrypi:/dev/shm $ cd /lib/modules/5.4.77.-v8+/extra
    pi@raspberrypi:/lib/modules/5.4.77.-v8+/extra $ sudo depmod
    ```

- depmod should create this line in modules.dep
    ```console
    pi@raspberrypi:/lib/modules/5.10.46-v8+/extra $ cat ../modules.dep | grep ess_iio_moc.ko 
    extra/ess_iio_moc.ko: kernel/drivers/iio/industrialio.ko
    ```


- Edit rpi /boot/config.txt such that device-tree overlay applied on next boot
    ```console
    dtdebug=1
    dtoverlay=ess-iio-moc
    ```

## driver test app

- After driver module interrogate files

- device nodes
   ```console
    root@raspberrypi:/sys/bus/iio/devices# ls /dev/iio*
    /dev/iio:device0
    ```

- sysfs iio bus
   ```console
    pi@raspberrypi:~ $ ls -l /sys/bus/iio/devices/
    total 0
    lrwxrwxrwx 1 root root 0 Jun 30 17:18 iio:device0 -> ../../../devices/platform/soc/soc:ess-iio-moc/iio:device0
    ```

- sysfs iio bus device attributes of interest:

    - out_voltage<channel>_raw - raw channel
    - out_voltage_scale - scale shared by all channels
    - scan_elements - channels supporting buffers
    - trigger - associate trigger with sysfs trigger, interrupt, or hrtimer source

    ```console
        pi@raspberrypi:~ $ ls -l  /sys/bus/iio/devices/iio\:device0/
        total 0
        drwxr-xr-x 2 root root    0 Jul  4 17:07 buffer
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 current_timestamp_clock
        -r--r--r-- 1 root root 4096 Jul  4 17:07 dev
        -r--r--r-- 1 root root 4096 Jul  4 17:07 name
        lrwxrwxrwx 1 root root    0 Jul  4 17:07 of_node -> ../../../../../firmware/devicetree/base/soc/ess-iio-moc
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 out_voltage0_raw
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 out_voltage1_raw
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 out_voltage2_raw
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 out_voltage3_raw
        -rw-r--r-- 1 root root 4096 Jul  4 17:07 out_voltage_scale
        drwxr-xr-x 2 root root    0 Jul  4 17:07 power
        drwxr-xr-x 2 root root    0 Jul  4 17:07 scan_elements
        lrwxrwxrwx 1 root root    0 Jul  4 17:07 subsystem -> ../../../../../bus/iio
        drwxr-xr-x 2 root root    0 Jul  4 17:07 trigger
        -rw-r--r-- 1 root root 4096 Jul  4 17:06 uevent
    ```

- inspect scan elements. "_en" extension indicates if data will be present in a triggered capture.
    ``` console
    pi@raspberrypi:~ $ ls -l  /sys/bus/iio/devices/iio\:device0/scan_elements/
    total 0
    -rw-r--r-- 1 root root 4096 Jul  4 17:56 in_timestamp_en
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_timestamp_index
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_timestamp_type
    -rw-r--r-- 1 root root 4096 Jul  4 17:56 in_voltage0_en
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage0_index
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage0_type
    -rw-r--r-- 1 root root 4096 Jul  4 17:56 in_voltage1_en
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage1_index
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage1_type
    -rw-r--r-- 1 root root 4096 Jul  4 17:56 in_voltage2_en
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage2_index
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage2_type
    -rw-r--r-- 1 root root 4096 Jul  4 17:56 in_voltage3_en
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage3_index
    -r--r--r-- 1 root root 4096 Jul  4 17:56 in_voltage3_type
    ```

- scan element _type indicate format of data. in_voltage0 will be 8 active bits out of 8, requiring a 2 bit shift.
    ```console
    pi@raspberrypi:~ $ cat  /sys/bus/iio/devices/iio\:device0/scan_elements/in_voltage0_type 
    le:s6/8>>2
    ```

- All channels are in. Read and verify moc incrementing value.
    ```console
    pi@raspberrypi:~ $ cat  /sys/bus/iio/devices/iio\:device0/in_voltage0_raw 
    0
    pi@raspberrypi:~ $ cat  /sys/bus/iio/devices/iio\:device0/in_voltage0_raw 
    1
    ```

- Channel scales. Multiply value raw value by scale to obtain scaled output.
    ```console
    pi@raspberrypi:~ $ cat /sys/bus/iio/devices/iio\:device0/in_voltage0_scale 
    0.250000
    pi@raspberrypi:~ $ cat /sys/bus/iio/devices/iio\:device0/in_voltage1_scale 
    1.500000
    ```

- Test sysfs triggering. Unable to sudo the echo directly. Needed to sudo su and then execute operations on the sysfs iio trigger. Test below adds to triggers with names sysfstrig5 and sysfstrig6.
    ```console
    pi@raspberrypi:/sys/bus/iio/devices $ sudo echo 5 > iio_sysfs_trigger/add_trigger 
    -bash: iio_sysfs_trigger/add_trigger: Permission denied
    pi@raspberrypi:/sys/bus/iio/devices $ sudo su
    root@raspberrypi:/sys/bus/iio/devices# su echo 5 > iio_sysfs_trigger/add_trigger
    su: user echo does not exist
    root@raspberrypi:/sys/bus/iio/devices# echo 5 > iio_sysfs_trigger/add_trigger
    root@raspberrypi:/sys/bus/iio/devices# echo 6 > iio_sysfs_trigger/add_trigger
    root@raspberrypi:/sys/bus/iio/devices# ls -l iio_sysfs_trigger
    lrwxrwxrwx 1 root root 0 Jul  3 16:10 iio_sysfs_trigger -> ../../../devices/iio_sysfs_trigger
    root@raspberrypi:/sys/bus/iio/devices# ls -l iio_sysfs_trigger/
    total 0
    --w------- 1 root root 4096 Jul  3 16:56 add_trigger
    drwxr-xr-x 2 root root    0 Jul  3 16:17 power
    --w------- 1 root root 4096 Jul  3 16:55 remove_trigger
    lrwxrwxrwx 1 root root    0 Jul  3 16:17 subsystem -> ../../bus/iio
    drwxr-xr-x 3 root root    0 Jul  3 16:56 trigger0
    drwxr-xr-x 3 root root    0 Jul  3 16:56 trigger1
    -rw-r--r-- 1 root root 4096 Jul  3 16:10 uevent
    root@raspberrypi:/sys/bus/iio/devices# cat iio_sysfs_trigger/trigger0/name 
    sysfstrig5
    ```

- Still as root, assign sysfs trigger 'sysfstrig5' to our iio:device0 device 'trigger' attribute.
    ```console
    root@raspberrypi:/home/pi# echo sysfstrig5 > /sys/bus/iio/devices/iio:device0/trigger/current_trigger
    root@raspberrypi:/home/pi# cat /sys/bus/iio/devices/iio:device0/trigger/current_trigger
    sysfstrig5
    ```

- Enable some channels to be buffered by trigger.
    ```console
    root@raspberrypi:/home/pi# echo 1 > /sys/bus/iio/devices/iio:device0/scan_elements/in_voltage0_en
    root@raspberrypi:/home/pi# echo 1 > /sys/bus/iio/devices/iio:device0/scan_elements/in_voltage2_en
    ```



- iio_info utility
    ```console
    root@raspberrypi:/sys/bus/iio/devices# /home/pi/Downloads/arm/usr/bin/iio_info 
    Library version: 0.16 (git tag: v0.16)
    Compiled with backends: local xml ip usb serial
    IIO context created with local backend.
    Backend version: 0.16 (git tag: v0.16)
    Backend description string: Linux raspberrypi 5.10.46-v8+ #1 SMP PREEMPT Thu Jul 1 20:02:37 UTC 2021 aarch64
    IIO context has 1 attributes:
            local,kernel: 5.10.46-v8+
    IIO context has 4 devices:
            iio:device0: ess_iio_moc
                    4 channels found:
                            voltage0:  (input)
                            2 channel-specific attributes found:
                                    attr  0: raw value: 
                                    attr  1: scale value: 
                            voltage1:  (input)
                            2 channel-specific attributes found:
                                    attr  0: raw value: 
                                    attr  1: scale value: 
                            voltage2:  (input)
                            2 channel-specific attributes found:
                                    attr  0: raw value: 
                                    attr  1: scale value: 
                            voltage3:  (input)
                            2 channel-specific attributes found:
                                    attr  0: raw value: 
                                    attr  1: scale value: 
                    No trigger on this device
            iio_sysfs_trigger:
                    0 channels found:
                    2 device-specific attributes found:
                                    attr  0: add_trigger ERROR: Permission denied (-13)
                                    attr  1: remove_trigger ERROR: Permission denied (-13)
                    No trigger on this device
            trigger0: sysfstrig5
                    0 channels found:
                    1 device-specific attributes found:
                                    attr  0: trigger_now ERROR: Permission denied (-13)
                    No trigger on this device
            trigger1: sysfstrig6
                    0 channels found:
                    1 device-specific attributes found:
                                    attr  0: trigger_now ERROR: Permission denied (-13)
                    No trigger on this device
    ```

- Build test app on target
    ```console
    pi@raspberrypi: g++ -g -O0 -std=c++17 -ggdb -lpthread -o ...
    ```

- Load, test, unload module. Note: modprobe picks up driver registered industrioio dependency. Would need to insmod instustrialio before insmod'ing ess_iio_moc otherwise. 
```console
pi@raspberrypi: dmesg -wH
pi@raspberrypi: sudo modprobe ess_iio_moc
pi@raspberrypi: <tun test app>
pi@raspberrypi: sudo rmmod ess_iio_moc.ko
```

### detailed docs
