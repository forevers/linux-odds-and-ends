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
    // Q: do I need dts for probe designs?
    // notes:
    $ cat /lib/modules/<version/modules.dep had a module dependency list ... probably part of a formal make install

    $ dtc -W no-unit_address_vs_reg -I dts -O dtb -o ess-iio-moc.dtbo ess-iio-moc-overlay.dts

    // https://stackoverflow.com/questions/34800731/module-not-found-when-i-do-a-modprobe
    // also placed ko in /lib/modules/5.4.77.-v8+/extra, ran sudo depmod, then sudo modprobe ess_iio_moc,
    //    and still no probe prints
*/

#define INIT_EXIT_MACRO
// #define USE_TRIGGER

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

/* see include/uapi/linux/iio/types.h for .type iio_chan_type enumeration */
#define MOC_VOLTAGE_CHANNEL(num) \
    { \
        .type = IIO_VOLTAGE, \
        .indexed = 1, \
        .channel = (num), \
        .address = (num), \
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) \
    }

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
    MOC_VOLTAGE_CHANNEL(0),
    MOC_VOLTAGE_CHANNEL(1),
    MOC_VOLTAGE_CHANNEL(2),
    MOC_VOLTAGE_CHANNEL(3),
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
    indio_dev->available_scan_masks = (const unsigned long *)0xF;

#if defined(USE_TRIGGER)
    PR_INFO("pre iio_triggered_buffer_setup()");
    /* enable trigger buffer support */
    if (0 > (ret = iio_triggered_buffer_setup(indio_dev,
            iio_pollfunc_store_time,
            moc_trigger_handler,
            NULL))) {
        dev_err(&pdev->dev, "moc iio_triggered_buffer_setup() failure");

        /* free iio device memory */
        iio_device_free(indio_dev);
        return ret;
    }
    PR_INFO("pose iio_triggered_buffer_setup()");
#endif

    /* register device with iio subsystem */
    iio_device_register(indio_dev);
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
    // int ret;
    // u8 range_idx;
    struct private_data *data = iio_priv(indio_dev);

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        // ret = bma220_read_reg(data->spi_device, chan->address);
        // if (ret < 0)
        // 	return -EINVAL;
        // *val = sign_extend32(ret >> BMA220_DATA_SHIFT, 5);
        *val = data->moc_data;
        return IIO_VAL_INT;
    // case IIO_CHAN_INFO_SCALE:
    // 	ret = bma220_read_reg(data->spi_device, BMA220_REG_RANGE);
    // 	if (ret < 0)
    // 		return ret;
    // 	range_idx = ret & BMA220_RANGE_MASK;
    // 	*val = bma220_scale_table[range_idx][0];
    // 	*val2 = bma220_scale_table[range_idx][1];
    // 	return IIO_VAL_INT_PLUS_MICRO;
    }

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

    /* unregister device from iio subsystem */
    iio_device_unregister(indio_dev);

    /* free iio device memory */
    iio_device_free(indio_dev);

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

    for (moc_idx = 0; moc_idx < ARRAY_SIZE(moc_channels) - 1; moc_idx++) {
        data->buffer[moc_idx] = moc_idx + data->moc_data;
    }
    data->moc_data++;

    iio_push_to_buffers_with_timestamp(indio_dev, data->buffer, pf->timestamp);

    mutex_unlock(&data->lock);
    iio_trigger_notify_done(indio_dev->trig);

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
        // .of_match_table = of_match_ptr(iio_moc_ids),
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