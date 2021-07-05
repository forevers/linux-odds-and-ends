#include <linux/bits.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "../utils/util.h"

/*
    build syntax:
        cross :
            $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C <path to cross compiled kernel>/linux M=$(pwd) modules
        native :
            $ make -c /lib/modules/$(uname -r)/build M=$(pwd) modules

    references:
        https://www.kernel.org/doc/html/v4.14/driver-api/iio
        'Linux Device Device Drivers', John Madieu

    // debuging:
    // consider adding MODULE_SOFTDEP("post: industrialio") to this module
    // didn't seem to have a effect
    // iio methods not available
    // extract all the non-stack symbols from a kernel and build a data blob
    $ cat /proc/kallsyms | grep iio_device_register
    $ sudo modprobe industrialio
    // now it is present and insmod runs without error but nothing under /sys/bus/iio/devices and no probe prints
    // notes:
    $ cat /lib/modules/<version/modules.dep had a module dependency list ... probably part of a formal make install

    $ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-iio-moc.dtbo ess-iio-moc-overlay.dts

    // https://stackoverflow.com/questions/34800731/module-not-found-when-i-do-a-modprobe
    // also placed ko in /lib/modules/5.4.77.-v8+/extra, ran sudo depmod, then sudo modprobe ess_iio_moc
*/

#define INIT_EXIT_MACRO
#define USE_TRIGGER

static int
moc_read_raw(
    struct iio_dev *indio_dev,
    struct iio_chan_spec const *chan,
    int *val,
    int *val2,
    long mask);

static int
moc_write_raw(
    struct iio_dev *indio_dev,
    struct iio_chan_spec const *channel,
    int val,
    int val2,
    long mask);

#define MOC_DATA_SHIFT  2

/*  .type = IIO_VOLTAGE - "voltage" used in sysfs name
    .output - 1 output ("out" used is sysfs name), 0 input ("in" used is sysfs name)
    .address -
    .indexed = 1 - .channel used in sysfs name
    .channel - used in sysfs name
    scan_type - signed, 
                6 valid data bits in 8 bit payload
                2 bit shits required

    see include/uapi/linux/iio/types.h for .type iio_chan_type enumeration 
*/
#if defined(USE_TRIGGER)
#define MOC_VOLTAGE_CHANNEL(index, is_output) { \
    .type = IIO_VOLTAGE, \
    .output = is_output, \
    .address = (index), \
    .indexed = 1, \
    .channel = (index), \
    .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE), \
    .scan_index = index, \
    .scan_type = { \
        .sign = 's', \
        .realbits = 6, \
        .storagebits = 8, \
        .shift = MOC_DATA_SHIFT, \
        .endianness = IIO_CPU, \
    }, \
}
#else
#define MOC_VOLTAGE_CHANNEL(index) \
    { \
        .type = IIO_VOLTAGE, \
        .address = (index), \
        .indexed = 1, \
        .channel = (index), \
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) \
    }
#endif


struct private_data {
    int foo;
    int bar;
#if defined(USE_TRIGGER)
    s8 buffer[12]; /* 4x8-bit channels + 8x8 timestamp */
    u8 moc_data; 
#endif
    struct mutex lock;
};

static const struct iio_chan_spec moc_channels[] = {
    MOC_VOLTAGE_CHANNEL(0, 0),
    MOC_VOLTAGE_CHANNEL(1, 0),
    MOC_VOLTAGE_CHANNEL(2, 0),
    MOC_VOLTAGE_CHANNEL(3, 0),
#if defined(USE_TRIGGER)
    IIO_CHAN_SOFT_TIMESTAMP(4),
#endif
};

static const struct iio_info moc_iio_info = {
    .read_raw = moc_read_raw,
    .write_raw = moc_write_raw,
};

#if defined(USE_TRIGGER)
static irqreturn_t moc_trigger_handler(int, void *);
#endif

/* chanel voltage ranges */
static const int moc_range[][2] = {
	{0, 250000}, {1, 500000}, {2, 750000}, {3, 000000},
};


static int
moc_iio_probe(struct platform_device *pdev)
{
    struct iio_dev *indio_dev;
    struct private_data *data;
#if defined(USE_TRIGGER)
    int ret;
#endif

    PR_INFO("entry");

#if defined (USE_TRIGGER)
    PR_INFO("triggering supported");
#endif

    /* allocate memory for iio device */
    indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*data));
    if (!indio_dev) {
        dev_err(&pdev->dev, "devm_iio_device_alloc() failure\n");
        return -ENOMEM;
    }

    /* address of iio device memory */
    data = iio_priv(indio_dev);
#if defined(USE_TRIGGER)
    data->moc_data = 0;
#endif

    mutex_init(&data->lock);
    
    indio_dev->dev.parent = &pdev->dev;
    indio_dev->info = &moc_iio_info;
    indio_dev->name = KBUILD_MODNAME;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = moc_channels;
    indio_dev->num_channels = ARRAY_SIZE(moc_channels);
    /* 4 moc channel available */
    indio_dev->available_scan_masks = (const unsigned long *)0xF;

#if defined(USE_TRIGGER)
    PR_INFO("pre iio_triggered_buffer_setup()");
    /* enable trigger buffer support */
    if (0 > (ret = iio_triggered_buffer_setup(indio_dev,
            iio_pollfunc_store_time,
            moc_trigger_handler,
            NULL))) {
        dev_err(&pdev->dev, "moc iio_triggered_buffer_setup() failure");
        return ret;
    }
    PR_INFO("post iio_triggered_buffer_setup()");
#endif

    /* register device with iio subsystem */
    PR_INFO("pre iio_device_register()");
    if (0 != (ret = iio_device_register(indio_dev))) {
        dev_err(&pdev->dev, "moc iio_device_register() failure: %d", ret);

        /* free iio device memory */
        iio_triggered_buffer_cleanup(indio_dev);
        return ret;
    }
    PR_INFO("pre platform_set_drvdata()");
    platform_set_drvdata(pdev, indio_dev);

    PR_INFO("exit");

    return 0;
}


/* callback for iio device sysfs attribute read */
static int
moc_read_raw(
    struct iio_dev *indio_dev,
    struct iio_chan_spec const *chan,
    int *val,
    int *val2,
    long mask)
{
#if defined(USE_TRIGGER)
    struct private_data *data = iio_priv(indio_dev);

    PR_INFO("entry");

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        PR_INFO("IIO_CHAN_INFO_RAW");
        *val = data->moc_data++;
        PR_INFO("exit");
        return IIO_VAL_INT;
    case IIO_CHAN_INFO_SCALE:
    	*val = moc_range[chan->channel][0];
    	*val2 = moc_range[chan->channel][1];
    	return IIO_VAL_INT_PLUS_MICRO;
    }

    PR_INFO("exit");
    return -EINVAL;
#else
    PR_INFO("entry");
    PR_INFO("exit");
    return 0;
#endif
}


static int
moc_write_raw(
    struct iio_dev *indio_dev,
    struct iio_chan_spec const *channel,
    int val,
    int val2,
    long mask)
{
    PR_INFO("entry");
    PR_INFO("exit");
    return 0;
}


static int moc_iio_remove(struct platform_device *pdev)
{
    struct iio_dev *indio_dev;

    PR_INFO("entry");

    indio_dev = platform_get_drvdata(pdev);

    PR_INFO("pre iio_device_unregister()");

    /* unregister device from iio subsystem */
    iio_device_unregister(indio_dev);
    iio_triggered_buffer_cleanup(indio_dev);

    PR_INFO("pre iio_device_free()");

    PR_INFO("exit");

    return 0;
}


#if defined(USE_TRIGGER)
static irqreturn_t 
moc_trigger_handler(int irq, void *p)
{
    struct iio_poll_func *pf = p;
    struct iio_dev *indio_dev = pf->indio_dev;
    struct private_data *data = iio_priv(indio_dev);
    u8 moc_idx = 0;

    mutex_lock(&data->lock);

    PR_INFO("isr 0");

    for (moc_idx = 0; moc_idx < ARRAY_SIZE(moc_channels) - 1; moc_idx++) {
        data->buffer[moc_idx] = moc_idx + data->moc_data;
    }
    data->moc_data++;
    PR_INFO("isr 1");

    iio_push_to_buffers_with_timestamp(indio_dev, data->buffer, pf->timestamp);

    PR_INFO("isr 2");

    mutex_unlock(&data->lock);

    PR_INFO("isr 3");

    iio_trigger_notify_done(indio_dev->trig);

    PR_INFO("isr 4");

    return IRQ_HANDLED;
}
#endif

static const struct of_device_id iio_moc_ids[] = {
    {.compatible = "ess,ess-iio-moc"},
    {}
};

MODULE_DEVICE_TABLE(of, iio_moc_ids);


static struct platform_driver ess_iio_moc_driver = {
    .probe = moc_iio_probe,
    .remove = moc_iio_remove,
    .driver = {
        .name = "ess-iio-moc",
        .of_match_table = iio_moc_ids,
        .owner = THIS_MODULE,
    },
};


#if defined(INIT_EXIT_MACRO)
module_platform_driver(ess_iio_moc_driver)
#else
// static int __init ess_oled_init(void)
int ess_iio_init(void) 
{
    int ret = 0;
    PR_INFO("entry");

    if (0 != platform_driver_register(&ess_iio_moc_driver)) {
        PR_ERR("platform_driver_register() failure");
    }

    PR_INFO("exit");
    return ret;
}
module_init(ess_iio_init);
// static void __exit ess_oled_cleanup(void)
void ess_iio_cleanup(void)
{
    PR_INFO("entry");

    platform_driver_unregister(&ess_iio_moc_driver);

    PR_INFO("exit");
}
module_exit(ess_iio_cleanup);
#endif

MODULE_AUTHOR("Developer Name <developer_email>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("a canonical driver template");
MODULE_VERSION("0.1");
/* driver depends on industrialio module */
MODULE_SOFTDEP("post: industrialio");