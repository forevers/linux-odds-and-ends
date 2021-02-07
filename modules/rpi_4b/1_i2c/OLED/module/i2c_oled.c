/* 
    i2c driver module for the Adafruit SSD1306

    general references:
        i2c/smbus:
            https://www.kernel.org/doc/Documentation/i2c/functionality
            /Documentation/i2c/writing-clients.rst
        gpio dts:
            https://www.kernel.org/doc/Documentation/devicetree/bindings/gpio/gpio.txt
        SSD1305 data sheet:
            https://cdn-shop.adafruit.com/datasheets/SSD1305.pdf
            https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
        Adafruit python:
            https://github.com/adafruit/Adafruit_CircuitPython_SSD1306
            https://github.com/adafruit/Adafruit_CircuitPython_framebuf/blob/master/adafruit_framebuf.py
        kernel support for ssD1305:
            /linux/arch/arm/boot/dts/imx6dl-yapp4-common.dtsi
            /linux/Documentation/devicetree/bindings/display/ssd1307fb.txt
            /linux/drivers/video/fbdev/ssd1307fb.c
            /linux/drivers/staging/fbtft/fb_ssd1305.c
        kernel memory barriers:
            http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4444.html
            https://lwn.net/Articles/718628/
            https://lwn.net/Articles/720550/
            https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/LWNLinuxMM/StrongModel.html
        gpio user space get:
            $ sudo raspi-gpio get
    
    echo privilege:
        // cmd 0x01 : display disable
        sudo bash -c 'echo -n -e \\x01 > /dev/ess-oled'
        // cmd 0x02 : display enable
        sudo bash -c 'echo -n -e \\xae > /dev/ess-oled'
        // cmd 0x05 : draw point at x, y
        sudo bash -c 'echo -n -e \\x05\\x40\\x10 > /dev/ess-oled'
        // cmd 0x06 : draw line at x, y
        sudo bash -c 'echo -n -e \\x05\\x40\\x10 > /dev/ess-oled'

    dts overlay:
        compile
            $ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-i2c-oled.dtbo ess-i2c-oled-overlay.dts
        copy dtbo into rpi /boot/overlays directory
            $ scp ess-i2c-oled.dtbo pi@xxx.xxx.xxx.xxx:/home/pi
            pi@raspberrypi:~ sudo mv ess-i2c-oled.dtbo /boot/overlays
            pi@raspberrypi:~/projects/oled/driver $ ls /boot/overlays/ | grep ess-*
            ess-i2c-oled.dtbo
        place into /boot/config.txt for auto load
            pi@raspberrypi:~/projects/oled/driver $ cat /boot/config.txt | grep ess-*
            # ess
            dtoverlay=ess-i2c-oled
        verify driver dts configured
            $ cat /proc/device-tree/soc/i2c@7e804000/ess-oled@3c/compatible
            ess,ess-oled
        inspect device tree binary
            $ fdtdump ess-i2c-oled.dtbo 

*/

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>          /* copy_(to,from)_user */

#include "gpio_oled_irq.h"
#include "i2c_oled_global.h"
#include "util.h"


typedef struct ssd1305_data {
    __u8* buffer;               /* raw display data + byte 0 prefix of 0x40 */
    __u8 height;
    __u8 pages;
    __u8 width;
} ssd1305_data;

typedef struct ess_ssd1305_oled {
    struct i2c_client* client;
    struct ssd1305_data oled_data;
} ess_ssd1305_oled;

static void rect(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 height, __u8 color, __u8 fill);

// from /linux/drivers/video/fbdev/ssd1307fb.c
// Co=1, D/C#=0
#define COMMAND     0x80

// from adafruit-circuitpython-ssd1306-2.9.2/adafruit_ssd1306.py
#define SET_CONTRAST 0x81
#define SET_ENTIRE_ON 0xA4
#define SET_NORM_INV 0xA6
#define SET_DISP_OFF 0xAE
#define SET_DISP_ON 0xAF
#define SET_MEM_ADDR 0x20
#define SET_COL_ADDR 0x21
#define SET_PAGE_ADDR 0x22
#define SET_DISP_START_LINE 0x40
#define SET_SEG_REMAP 0xA0
#define SET_MUX_RATIO 0xA8
#define SET_COM_OUT_DIR 0xC0
#define SET_DISP_OFFSET 0xD3
#define SET_COM_PIN_CFG 0xDA
#define SET_DISP_CLK_DIV 0xD5
#define SET_PRECHARGE 0xD9
#define SET_VCOM_DESEL 0xDB
#define SET_CHARGE_PUMP 0x8D

static struct i2c_client* user_i2c_client_;
static struct ess_ssd1305_oled* ess_ssd1305_oled_;

/* for read, write, and ioctl */
static void rect(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 height, __u8 color, __u8 fill);
static void set_pixel(struct ess_ssd1305_oled*, __u8 x, __u8 y, __u8 color);
static int get_pixel(struct ess_ssd1305_oled*, __u8 x, __u8 y);
static void hline(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 color);
static void vline(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 height, __u8 color);

/* utility methods */
static int write_cmd(struct i2c_client* client, u8 cmd);
static int write_cmd_data(struct i2c_client* client, u8 cmd, u8 data);
static int show(struct ess_ssd1305_oled* oled);
static void fill(struct ess_ssd1305_oled*, __u8 color);
static void fill_rect(struct ess_ssd1305_oled*, __u8 x, __u8 y, __u8 width, __u8 height, __u8 color);


/* render the entire frame buffer */
static int show(struct ess_ssd1305_oled* oled)
{
    int len;
    int ret = 0;

    /* Update the display */
    __u8 xpos0 = 0;
    __u8 xpos1 = oled->oled_data.width - 1;
    if ((ret = write_cmd(oled->client, SET_COL_ADDR)) < 0) {
        PR_ERR("write_cmd SET_COL_ADDR failure");
        return ret;
    }
    if ((ret = write_cmd(oled->client, xpos0)) < 0) {
        PR_ERR("write_cmd SET_COL_ADDR xpos failure");
        return ret;
    }
    if ((ret = write_cmd(oled->client, xpos1)) < 0) {
        PR_ERR("write_cmd SET_COL_ADDR xpos1 failure");
        return ret;
    }

    if ((ret = write_cmd(oled->client, SET_PAGE_ADDR)) < 0) {
        PR_ERR("write_cmd SET_PAGE_ADDR failure");
        return ret;
    }
    if ((ret = write_cmd(oled->client, 0)) < 0) {
        PR_ERR("write_cmd SET_PAGE_ADDR data failure");
        return ret;
    }
    if ((ret = write_cmd(oled->client, oled->oled_data.pages - 1)) < 0) {
        PR_ERR("write_cmd SET_PAGE_ADDR data failure");
        return ret;
    }

    // see /linux/drivers/video/fbdev/ssd1307fb.c
    //   ssd1307fb_update_display() for reference
    len = oled->oled_data.pages * oled->oled_data.width + 1;
    ret = i2c_master_send(oled->client, oled->oled_data.buffer, len);
    if (ret != len) {
        PR_ERR("i2c_master_send failure");
        dev_err(&oled->client->dev, "Couldn't send I2C command.\n");
        return ret;
    }

    return ret;
}


/* fill or clear framebuffer */
static void fill(struct ess_ssd1305_oled* oled, __u8 color)
{
    int i;
    __u8 fill_value = 0x00;

    PR_INFO("entry");

    if (color) fill_value = 0xFF;

    PR_INFO("data->pages: %d, data->width: %d, fill_value: %d", oled->oled_data.pages, oled->oled_data.width, fill_value);

    /* MVLSBFormat for single bit SD1305 display */
    for (i = 0; i < oled->oled_data.pages*oled->oled_data.width; ++i) {
        oled->oled_data.buffer[i+1] = fill_value;
    }

    PR_INFO("exit");
}


/* set a single pixel */
static void set_pixel(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 color)
{
    /*
    stride: The number of pixels between each horizontal line of pixels in the
            FrameBuffer. This defaults to ``width`` but may need adjustments when
            implementing a FrameBuffer within another larger FrameBuffer or screen. The
            ``buf`` size must accommodate an increased step size.
    */
    int index;
    int stride;
    int offset;

    /* Set a given pixel to a color */
    /* MVLSBFormat
       https://github.com/adafruit/Adafruit_CircuitPython_framebuf/blob/master/adafruit_framebuf.py
    */
    stride = oled->oled_data.width;
    index = (y >> 3) * stride + x;
    offset = y & 0x07;
    oled->oled_data.buffer[index+1] = (oled->oled_data.buffer[index+1] & ~(0x01 << offset)) | ((color != 0) << offset);
}


/* get a single pixel */
static int get_pixel(ess_ssd1305_oled* oled, __u8 x, __u8 y)
{
    int index;
    int offset;
    int stride;

    /* Get the color of a given pixel */
    /* MVLSBFormat
       https://github.com/adafruit/Adafruit_CircuitPython_framebuf/blob/master/adafruit_framebuf.py
    */
    stride = oled->oled_data.width;
    index = (y >> 3) * stride + x;
    offset = y & 0x07;
    return (oled->oled_data.buffer[index+1] >> offset) & 0x01;
}


/* fill a rectangle within the framebuffer */
static void fill_rect(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 height, __u8 color)
{
    int index;
    int stride = oled->oled_data.width;
    int offset;
    int w;

    /* Draw a rectangle at the given location, size and color */
    /* MVLSBFormat
       https://github.com/adafruit/Adafruit_CircuitPython_framebuf/blob/master/adafruit_framebuf.py
    */
    while (height > 0) {
        index = (y >> 3) * stride + x;
        offset = y & 0x07;
        for (w = 0; w < width; w++) {
            oled->oled_data.buffer[index + w + 1] = (oled->oled_data.buffer[index + w + 1] & ~(0x01 << offset)) | ((color != 0) << offset);
        }
        y += 1;
        height -= 1;
    }
}


/* fill or clear a rectangle within the frame buffer */
static void rect(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 height, __u8 color, __u8 fill)
{
    /* Draw a rectangle at the given location, size and color.
       The rect method draws only a 1 pixel outline.
    */

    int x_end;
    int y_end;

    if (width < 1 ||
        height < 1 ||
        (x + width) <= 0 ||
        (y + height) <= 0 ||
        y >= oled->oled_data.height ||
        x >= oled->oled_data.width)
        return;

    x_end = ((oled->oled_data.width - 1) < (x + width - 1)) ? (oled->oled_data.width - 1) : (x + width - 1);
    y_end = ((oled->oled_data.height - 1) < (y + height - 1)) ? (oled->oled_data.height - 1) : (y + height - 1);
    x = (x > 0) ? x : 0;
    y = (y > 0) ? y : 0;
    if (fill) {
        fill_rect(oled, x, y, x_end - x + 1, y_end - y + 1, color);
    } else {
        fill_rect(oled, x, y, x_end - x + 1, 1, color);
        fill_rect(oled, x, y, 1, y_end - y + 1, color);
        fill_rect(oled, x, y_end, x_end - x + 1, 1, color);
        fill_rect(oled, x_end, y, 1, y_end - y + 1, color);
    }
}


/* draw a horizontal line */
static void hline(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 width, __u8 color)
{
    /* Draw a horizontal line up to a given length */
    rect(oled, x, y, width, 1, color, 1);
}


/* draw a vertical line */
static void vline(ess_ssd1305_oled* oled, __u8 x, __u8 y, __u8 height, __u8 color)
{
    /* Draw a vertical line up to a given length */
    rect(oled, x, y, 1, height, color, 1);
}

/* i2c platform configuration */

static const struct i2c_device_id ess_oled_id[] = {
    { "ess_oled", 0 },
    { }
};
/* both of and platform (i2c in the case) device table can be implemented */ 
MODULE_DEVICE_TABLE(i2c, ess_oled_id);


/* Open Firwmare matching */
static const struct of_device_id ess_oled_of_match[] = {
    {.compatible = "ess,ess-oled"},
    {}
};
MODULE_DEVICE_TABLE(of, ess_oled_of_match);
/* $ cat /proc/device-tree/soc/i2c@7e804000/ess-oled@3c/compatible
   ess,ess-oled
*/


/* oled single command write */
static int write_cmd(struct i2c_client* client, u8 cmd)
{
    return i2c_smbus_write_byte_data(client, COMMAND, cmd);
}


/* oled command write with data value */
static int write_cmd_data(struct i2c_client* client, u8 cmd, u8 data)
{
    int ret;
    
    if ((ret = write_cmd(client, cmd)) == 0) {
        ret = write_cmd(client, data);
    }

    return ret;
}


/* probe perform oled initialization */
static int ess_oled_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
    /* /linux/include/uapi/linux/i2c.h for ic2/smbus adapter supported functionality

        I2C_FUNC_I2C
            Plain i2c-level commands
        I2C_FUNC_SMBUS_BYTE_DATA
            Handles the SMBus read_byte_data and write_byte_data commands
        I2C_FUNC_SMBUS_READ_WORD_DATA
            Handles the SMBus read_word_data command
        ...
    */
    int ret;
    __u8 data;
    const struct of_device_id* match;

    PR_INFO("ess_oled_probe() entry");

    // match = i2c_of_match_device(ess_oled_of_match, &client->dev);
    match = of_match_device(ess_oled_of_match, &client->dev);
    if (match) {
        PR_INFO("of match found");
        /* device tree code here */
    } else {
        PR_INFO("of match not found");
        // TODO support non dts driver version
        /* platform data code here */
        // pdata = dev_get_platdata(&client->dev);
    }

    /* plain i2c-level commands */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "I2C_FUNC_I2C check failed\n");
        return -ENXIO;
    }

    /* smbus read/write byte and read word */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        dev_err(&client->dev, "I2C_FUNC_SMBUS_BYTE_DATA | 32 check failed\n");
        return -EOPNOTSUPP;
    }

    /* oled device data */
    if ((ess_ssd1305_oled_ = kmalloc(sizeof(ess_ssd1305_oled), GFP_KERNEL))) {
        ess_ssd1305_oled_->oled_data.height = 64;//32; //TODO thought this was 64?;
        ess_ssd1305_oled_->oled_data.pages = DIV_ROUND_UP(ess_ssd1305_oled_->oled_data.height, 8);
        ess_ssd1305_oled_->oled_data.width = 128;
        if (!(ess_ssd1305_oled_->oled_data.buffer = kzalloc((ess_ssd1305_oled_->oled_data.width * ess_ssd1305_oled_->oled_data.pages) + 1, GFP_KERNEL))) {
            PR_ERR("kzalloc failure");
            kfree(ess_ssd1305_oled_);
            return -ENOMEM;
        }
        // Set first byte of data buffer to Co=0, D/C=1
        ess_ssd1305_oled_->oled_data.buffer[0] =  0x40;
        PR_INFO("kmalloc %d bytes", ess_ssd1305_oled_->oled_data.width * ess_ssd1305_oled_->oled_data.pages);
    } else {
        PR_ERR("kmalloc failure");
        return -ENOMEM;
    }

    ess_ssd1305_oled_->client = client;
    i2c_set_clientdata(client, ess_ssd1305_oled_);

    /*  turn the OLED panel display ON */
    if ((ret = write_cmd(client, SET_DISP_ON)) < 0) {
        PR_ERR("write_cmd SET_DISP_ON failure");
        return ret;
    }

    /* OLED off */
    if ((ret = write_cmd(client, SET_DISP_OFF)) < 0) {
    // if ((ret = i2c_smbus_write_byte(client, SET_DISP_OFF)) < 0) {
        PR_ERR("write_cmd SET_DISP_OFF failure");
        return ret;
    }

    /* horizontal addressing (horizontal raster with row advances) */
    if ((ret = write_cmd_data(client, SET_MEM_ADDR, 0x00)) < 0) {
        PR_ERR("write_cmd SET_MEM_ADDR failure");
        return ret;
    }
    
    /* resolution and layout */
    /* line 0 */
    if ((ret = write_cmd(client, SET_DISP_START_LINE | 0x00)) < 0) {
        PR_ERR("write_cmd SET_DISP_START_LINE | 0x00 failure");
        return ret;
    }

    /* column addr 127 mapped to SEG0 */
    if ((ret = write_cmd(client, SET_SEG_REMAP | 0x01)) < 0) {
        PR_ERR("write_cmd SET_SEG_REMAP | 0x01 failure");
        return ret;
    }

    /* 16 to 63 (default val) */
    if ((ret = write_cmd_data(client, SET_MUX_RATIO, ess_ssd1305_oled_->oled_data.height-1)) < 0) {
        PR_ERR("write_cmd SET_MUX_RATIO failure");
        return ret;
    }

    /* scan from COM[N] to COM0 */
    if ((ret = write_cmd(client, SET_COM_OUT_DIR | 0x08)) < 0) {
        PR_ERR("write_cmd SET_COM_OUT_DIR | 0x08 failure");
        return ret;
    }

    /* set display offset */
    if ((ret = write_cmd_data(client, SET_DISP_OFFSET, 0x00)) < 0) {
        PR_ERR("write_cmd SET_DISP_OFFSET failure");
        return ret;
    };

    if (ess_ssd1305_oled_->oled_data.height == 32 || ess_ssd1305_oled_->oled_data.height == 16) {
        data = 0x02;
    } else {
        data = 0x12;
    }
    if ((ret = write_cmd_data(client, SET_COM_PIN_CFG, data)) < 0) {
        PR_ERR("write_cmd SET_COM_PIN_CFG failure");
        return ret;
    }

    /* timing and driving scheme */
    if ((ret = write_cmd_data(client, SET_DISP_CLK_DIV, 0x80)) < 0) {
        PR_ERR("write_cmd SET_DISP_CLK_DIV failure");
        return ret;
    }

    if ((ret = write_cmd_data(client, SET_PRECHARGE, 0xF1)) < 0) {
        PR_ERR("write_cmd SET_PRECHARGE failure");
        return ret;
    }

    if ((ret = write_cmd_data(client, SET_VCOM_DESEL, 0x30)) < 0) {
        PR_ERR("write_cmd SET_VCOM_DESEL failure");
        return ret;
    }

    /* display */
    if ((ret = write_cmd_data(client, SET_CONTRAST, 0xFF)) < 0) {
        PR_ERR("write_cmd SET_CONTRAST failure");
        return ret;
    }

    /* output follows RAM contents */
    if ((ret = write_cmd(client, SET_ENTIRE_ON)) < 0) {
        PR_ERR("write_cmd SET_ENTIRE_ON failure");
        return ret;
    }

    /* not inverted */
    if ((ret = write_cmd(client, SET_NORM_INV)) < 0) {
        PR_ERR("write_cmd SET_NORM_INV failure");
        return ret;
    }

    /* charge pump */
    if ((ret = write_cmd_data(client, SET_CHARGE_PUMP, 0x14)) < 0) {
        PR_ERR("write_cmd SET_CHARGE_PUMP failure");
        return ret;
    }

    /* OLED on */
    if ((ret = write_cmd(client, SET_DISP_ON)) < 0) {
        PR_ERR("write_cmd SET_DISP_ON failure");
        return ret;
    }

    if ((ret = show(ess_ssd1305_oled_)) < 0) {
        PR_ERR("show() failure");
        return ret;
    }

    /* Set a pixel in the origin 0, 0 position */
    set_pixel(ess_ssd1305_oled_, 0, 0, 1);
    /* Set a pixel in the middle 64, 32 position */
    set_pixel(ess_ssd1305_oled_, 64, 32, 1);
    /* Set a pixel in the opposite 127, 63 position */
    set_pixel(ess_ssd1305_oled_, 127, 63, 1);
    if ((ret = show(ess_ssd1305_oled_)) < 0) {
        PR_ERR("show() failure");
        return ret;
    }

    msleep(1000);

    rect(ess_ssd1305_oled_, 10, 10, 20, 10, 1, 1);
    if ((ret = show(ess_ssd1305_oled_)) < 0) {
        PR_ERR("show() failure");
        return ret;
    }

    msleep(1000);

    hline(ess_ssd1305_oled_, 31, 11, 10, 1);
    if ((ret = show(ess_ssd1305_oled_)) < 0) {
        PR_ERR("show() failure");
        return ret;
    }

    msleep(1000);

    vline(ess_ssd1305_oled_, 31, 11, 10, 1);
    if ((ret = show(ess_ssd1305_oled_)) < 0) {
        PR_ERR("show() failure");
        return ret;
    }

    /* cache client for fops */
    user_i2c_client_ = client;

    PR_INFO("ess_oled_probe() successful exit");

    return 0;
}


/* cleanup any probe configurations */
static int ess_oled_remove(struct i2c_client *client)
{
    ess_ssd1305_oled* ssd1305_oled;

    PR_INFO("ess_oled_remove() entry");

    /* oled device data */
    ssd1305_oled = i2c_get_clientdata(client);
    kfree(ssd1305_oled->oled_data.buffer);
    kfree(ssd1305_oled);

    PR_INFO("ess_oled_remove() exit");
    return 0;
}


/* fops read */
ssize_t ess_oled_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    ssize_t size;
    PR_INFO("entry");
    size = gpio_oled_irq_read(f, buff, count, pos);
    PR_INFO("exit");
    return size;
    //return 0;
}


__poll_t ess_oled_poll(struct file *f, struct poll_table_struct *wait)
{
    __poll_t poll;
    PR_INFO("entry");
    poll = gpio_oled_irq_poll(f, wait);
    PR_INFO("exit");
    return poll;
}


/* fops write */
ssize_t ess_oled_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
{
    int ret;
    // PR_INFO("entry");

    // PR_INFO("buff[0] = %x, count = %lx", buff[0], count);

    /* single byte write are REG writes */
    if (count == 1) {

        __u8 data[1];

        /* copy data from user space */
        if (copy_from_user(data, buff, 1)) {
            PR_ERR("copy_from_user() failed\n");
            return count;
        }

        switch(buff[0]) {
            case CMD_DISABLE_SEQ_NUM:
                ret = write_cmd(ess_ssd1305_oled_->client, SET_DISP_OFF);
                break;
            case CMD_ENABLE_SEQ_NUM:
                ret = write_cmd(ess_ssd1305_oled_->client, SET_DISP_ON);
                break;
            case CMD_FILL_BUFFER_SEQ_NUM:
                /* fill and update the display */
                fill(ess_ssd1305_oled_, 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
            case CMD_CLEAR_BUFFER_SEQ_NUM:
                /* fill and update the display */
                fill(ess_ssd1305_oled_, 0);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
        }

    } else if (count == 3) {

        __u8 data[3];

        /* copy data from user space */
        if (copy_from_user(data, buff, count)) {
            PR_ERR("copy_from_user() failed\n");
            return count;
        }

        switch(buff[0]) {
            case CMD_SET_PIXEL:
                set_pixel(ess_ssd1305_oled_, buff[PIXEL_X_IDX], buff[PIXEL_Y_IDX], 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
        }

    } else if (count == 4) {

        __u8 data[4];

        /* copy data from user space */
        if (copy_from_user(data, buff, count)) {
            PR_ERR("copy_from_user() failed\n");
            return count;
        }
        PR_INFO("buff[0] = %d-%d-%d-%d , count = %lx", buff[0], buff[1], buff[2], buff[3], count);

        switch(buff[0]) {
            case CMD_H_LINE:
                hline(ess_ssd1305_oled_, buff[LINE_X_IDX], buff[LINE_Y_IDX], buff[LINE_LEN_IDX], 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
            case CMD_V_LINE:
                vline(ess_ssd1305_oled_, buff[LINE_X_IDX], buff[LINE_Y_IDX], buff[LINE_LEN_IDX], 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
        }

    } else if (count == 5) {

        __u8 data[5];

        /* copy data from user space */
        if (copy_from_user(data, buff, count)) {
            PR_ERR("copy_from_user() failed\n");
            return count;
        }

        switch(buff[0]) {
            case CMD_RECT_FILL:
                rect(ess_ssd1305_oled_, buff[RECT_X_IDX], buff[RECT_Y_IDX], buff[RECT_WIDTH], buff[RECT_HEIGHT], 1, 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
            case CMD_RECT_CLEAR:
                rect(ess_ssd1305_oled_, buff[RECT_X_IDX], buff[RECT_Y_IDX], buff[RECT_WIDTH], buff[RECT_HEIGHT], 0, 1);
                if ((ret = show(ess_ssd1305_oled_)) < 0) {
                    PR_ERR("show() failure, ret: %d", ret);
                }
                break;
        }
    }

    // PR_INFO("exit");
    return count;
}


long ess_oled_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    long retval = 0;
    int ret = 0;
    PR_INFO("entry");
    
    PR_INFO("cmd: %x, arg: %lx", cmd, arg);

    switch(cmd) {

        case IOCTL_DISABLE:
            if ((ret = write_cmd(ess_ssd1305_oled_->client, SET_DISP_OFF)) < 0) {
                PR_ERR("SET_DISP_OFF ret: %d", ret);
            }
            break;
        case IOCTL_ENABLE:
            if ((ret = write_cmd(ess_ssd1305_oled_->client, SET_DISP_ON)) < 0) {
                PR_ERR("SET_DISP_ON failure, ret: %d", ret);
            }
            break;
        case IOCTL_FILL_BUFFER:
            /* fill buffer and update the display */
            fill(ess_ssd1305_oled_, 1);
            if ((ret = show(ess_ssd1305_oled_)) < 0) {
                PR_ERR("show() failure, ret: %d", ret);
            }
            break;
        case IOCTL_CLEAR_BUFFER:
            /* clear buffer and update the display */
            fill(ess_ssd1305_oled_, 0);
            if ((ret = show(ess_ssd1305_oled_)) < 0) {
                PR_ERR("show() failure, ret: %d", ret);
            }
            break;
        case IOCTL_RELEASE_POLL:
            break;
        default:

            retval = -EPERM;
            break;

        return retval;
    }

    PR_INFO("exit");
    return retval;
}


static struct i2c_driver ess_oled_driver = {
    .driver = {
        .name = "ess_oled_driver",
        .of_match_table = of_match_ptr(ess_oled_of_match),
    },
    .probe = ess_oled_probe,
    .remove = ess_oled_remove,
    .id_table = ess_oled_id,
};


#if defined(INIT_EXIT_MACRO)
/* creates init/exit functions 
   platform_driver_register() and platform_driver_un register() automatically called */
module_i2c_driver(ess_oled_driver);
#else
// static int __init ess_oled_init(void)
int ess_oled_init(void) 
{
    int ret;
    PR_INFO("entry");

    if ((ret = i2c_add_driver(&ess_oled_driver) == 0)) {
        if ((ret = gpio_oled_irq_init() != 0)) {
            PR_ERR("gpio_oled_irq_init() failure");
        }
    } else {
        PR_ERR("i2c_add_driver() failure");
    }

    PR_INFO("exit");
    return ret;
}
// module_init(ess_oled_init);
// static void __exit ess_oled_cleanup(void)
void ess_oled_cleanup(void)
{
    PR_INFO("ess_oled_cleanup() entry");
    i2c_del_driver(&ess_oled_driver);
    gpio_oled_irq_exit();
}
// module_exit(ess_oled_cleanup);
#endif
