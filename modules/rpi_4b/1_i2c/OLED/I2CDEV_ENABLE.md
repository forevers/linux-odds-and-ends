# rpi 4 I2CDev enable 
For Python and other user space i2c access
```console
pi@raspberrypi: sudo raspi-config
```
Select 'Interface Options -> I2C -> Enable'
```console
pi@raspberrypi: sudo reboot
```
Detect devices on /dev/i2c-1 device node
```console
pi@raspberrypi: sudo i2cdetect -y 1
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- --  
- 0x3c is OLED i2c address (0x78 shift by one on analyzer)
```