# Device Tree Overlay

- linux documentation
    - https://www.kernel.org/doc/Documentation/devicetree/overlay-notes.txt

- rpi documentation
    - /mnt/data/projects/rpi/clones/linux/arch/arm/boot/dts/overlays/README
    - https://www.raspberrypi.org/documentation/configuration/device-tree.md
    - Warning (unit_address_vs_reg): Node /fragment@0 has a unit name, but no reg property
        - https://www.raspberrypi.org/forums/viewtopic.php?f=107&t=161771&p1051588#p1051588
        "-W no-unit_address_vs_reg"

- compile
    ```console
    $ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-i2c-oled.dtbo ess-i2c-oled-overlay.dts
    ```

- dump dtb interrogation
    ```console
    $ fdtdump ess-i2c-oled.dtbo
    ```

- copy dtbo into rpi /boot/overlays directory
    ```console
    $ scp ess-i2c-oled.dtbo pi@xxx.xxx.xxx.xxx:/home/pi
    ```
    ```console
    pi@raspberrypi:~ sudo chown root: ess-i2c-oled.dtbo
    pi@raspberrypi:~ sudo chmod 755 ess-i2c-oled.dtbo
    pi@raspberrypi:~ sudo mv ess-i2c-oled.dtbo /boot/overlays
    ```
- edit /boot/config.txt
    ```console
    # ess oled bonnet driver
    dtdebug=1
    dtoverlay=ess-i2c-oled
    ```

- reboot

- verify overlay loaded
    ```console
    $ pi@raspberrypi:~$ sudo vcdbg log msg
        ...
        006053.724: dtdebug: Opened overlay file 'overlays/ess-i2c-oled.dtbo'
        006054.404: brfs: File read: /mfs/sd/overlays/ess-i2c-oled.dtbo
        006067.543: Loaded overlay 'ess-i2c-oled'
        006087.031: dtdebug: merge_fragment(/soc/i2c@7e804000,/fragment@0/__overlay__)
        006087.066: dtdebug:   +prop(#address-cells)
        006088.187: dtdebug:   +prop(#size-cells)
        006089.288: dtdebug:   +prop(status)
        006097.467: dtdebug: merge_fragment(/soc/i2c@7e804000/ess-oled@3c,/fragment@0/__overlay__/ess-oled@3c)
        006097.504: dtdebug:   +prop(compatible)
        006098.574: dtdebug:   +prop(status)
        006099.709: dtdebug:   +prop(#address-cells)
        006100.808: dtdebug:   +prop(#size-cells)
        006101.901: dtdebug:   +prop(reg)
        006103.051: dtdebug:   +prop(test_ref)
        006111.882: dtdebug: merge_fragment(/soc/i2c@7e804000/ess-oled@3c/test_subnode,/fragment@0/__overlay__/ess-oled@3c/test_subnode)
        006111.920: dtdebug:   +prop(dummy)
        006113.543: dtdebug:   +prop(phandle)
        006114.662: dtdebug: merge_fragment() end
        006114.696: dtdebug: merge_fragment() end
        006114.753: dtdebug: merge_fragment() end
        006129.225: brfs: File read: 635 bytes

    $ pi@raspberrypi:~ $ cat /proc/device-tree/soc/i2c@7e804000/ess-oled@3c/compatible 
    ess,ess-oled
    ```