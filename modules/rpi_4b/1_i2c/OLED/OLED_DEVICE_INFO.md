# OLED device info

## References:
- https://www.adafruit.com/product/3531?gclid=CjwKCAiA5IL-BRAzEiwA0lcWYmhQnqjrNGI3O4GhysnL0zKgbNk00GMMTXglSH1nolYW-BOGwMWQpBoCm8wQAvD_BwE
- https://learn.adafruit.com/adafruit-128x64-oled-bonnet-for-raspberry-pi

## Install CircuitPython 
- https://learn.adafruit.com/circuitpython-on-raspberrypi-linux/installing-circuitpython-on-raspberry-pi
    ```console
    pi@raspberrypi: sudo pip3 install --upgrade setuptools
    ```
## Configure python 3 as default version
- Version detection
    ```console
    pi@raspberrypi: python --version
    Python 2.7.16
    pi@raspberrypi: python3 --version
    Python 3.7.3
    ```

- https://raspberry-valley.azurewebsites.net/Python-Default-Version/
    ```console
    pi@raspberrypi: sudo update-alternatives --install /usr/bin/python python /usr/bin/python2.7 1
    pi@raspberrypi: sudo update-alternatives --install /usr/bin/python python /usr/bin/python3.7 2
    pi@raspberrypi: sudo update-alternatives --list python
    /usr/bin/python2.7
    /usr/bin/python3.7
    $ pi@raspberrypi: python --version
    Python 3.7.3
    ```

- To switch versions
    ```console
    $ sudo update-alternatives --config python
    ```

## Python libraries
- Python libraries available for high level design and testing
    ```console
    pi@raspberrypi: pip3 install RPI.GPIO
    pi@raspberrypi: pip3 install adafruit-blinka
    pi@raspberrypi: sudo pip3 install 
    ```
- adafruit-circuitpython-ssd1306: https://pypi.org/project/adafruit-circuitpython-ssd1306/

- python imaging library for font packages
    ```console
    - sudo apt-get install python3-pil
    ```

## OLED python example code

- adafruit source
    - https://learn.adafruit.com/pages/15682/elements/3024362/download?type=zip

- Simple button test
    ```console
    pi@raspberrypi: python ssd1306_bonnet_buttons.py
    ```

## ssd1306_simpletest.py notes

Reverse engineer some useful OLED controller configurations:

- /mnt/data/projects/rpi/i2c_devices/OLED_3531/examples/ssd1306_simpletest.py

- from board import SCL, SDA

    - interrogate in python shell for GPIO numbers for SCL and SDA
        ```console
        >>> from board import SCL, SDA
        >>> print(SCL)
        3
        >>> print(SDA)
        2
        ```

    - interrogate in python shell for i2c 'port'
        ```console
        >>> from microcontroller.pin import i2cPorts
        >>> print(i2cPorts)
        ((3, 3, 2), (1, 3, 2), (0, 1, 0))
        ```

Call stack:

- i2c = busio.I2C(SCL, SDA)
    - busio.I2C::__init__
        - init
            - detector.board.id=RASPBERRY_PI_4B
            - detector.chip.id=BCM2XXX
            - detector.board.any_embedded_linux=True
                - from adafruit_blinka.microcontroller.generic_linux.i2c import I2C as _I2C
                    - /pypi_library/Adafruit-Blinka-5.8.0/src/adafruit_blinka/microcontroller/generic_linux/i2c.py
                - from microcontroller.pin import i2cPorts
                    - /pypi_library/Adafruit-Blinka-5.8.0/src/microcontroller/pin.py
                        - elif chip_id == ap_chip.BCM2XXX:
                            from adafruit_blinka.microcontroller.bcm283x.pin import *
                            - /pypi_library/Adafruit-Blinka-5.8.0/src/adafruit_blinka/microcontroller/bcm283x/pin.py
                    - i2cPorts
                        i2cPorts = (
                            (3, SCL, SDA),
                            (1, SCL, SDA),
                            (0, D1, D0),  # both pi 1 and pi 2 i2c ports!
                        )
            - resolve board supported with requested i2c bus 
                - first portId with matching scl/sda gpio's is 1 (passes on the portId of 3)
                - for portId, portScl, portSda in i2cPorts:
                    try:
                        if scl == portScl and sda == portSda:
                            self._i2c = _I2C(portId, mode=_I2C.MASTER, baudrate=frequency)
                            break
            - construct i2c bus interface
                - self._i2c = _I2C(portId, mode=_I2C.MASTER, baudrate=frequency)
                    - for portId = 1, baudrate = =400000 (default)
                    - self._i2c_bus = smbus.SMBus(bus_num)
                        - has i2c specific bus commands, and ctype abstractions
                        - bus_num passed = 1
                        - def __init__(self, bus=None):
                            - self.open(bus)
                                - self._device = open("/dev/i2c-{0}".format(bus), "r+b", buffering=0)
- display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c)
    ```console
    def __init__(self, width, height, i2c, *, addr=0x3C, external_vcc=False, reset=None):
        ...
        super().__init__(
            memoryview(self.buffer)[1:],
            width,
            height,
            external_vcc=external_vcc,
            reset=reset,
        )

            class _SSD1306(framebuf.FrameBuffer):
                def __init__(self, buffer, width, height, *, external_vcc, reset):
                    super().__init__(buffer, width, height)
                    self.width = width
                    self.height = height
                    self.external_vcc = external_vcc
                    # reset may be None if not needed
                    self.reset_pin = reset
                    if self.reset_pin:
                        self.reset_pin.switch_to_output(value=0)
                    self.pages = self.height // 8
                    # Note the subclass must initialize self.framebuf to a framebuffer.
                    # This is necessary because the underlying data buffer is different
                    # between I2C and SPI implementations (I2C needs an extra byte).
                    self._power = False
                    self.poweron()
                        def poweron(self):
                            "Reset device and turn on the display."
                            self.write_cmd(SET_DISP | 0x01)
                                // derived class impls write
                                    def write_cmd(self, cmd):
                                        """Send a command to the SPI device"""
                                        self.temp[0] = 0x80  # Co=1, D/C#=0
                                        self.temp[1] = cmd
                                        with self.i2c_device:
                                            self.i2c_device.write(self.temp)

                            self._power = True
                    self.init_display()
    ```
