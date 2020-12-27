/* 
    build syntax :
        cross :
            $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C <path to cross compiled kernel>/linux M=$(pwd) modules
        native :
            $ make -c /lib/modules/$(uname -r)/build M=$(pwd) modules

    udev rules:
    // discover device udev parameters
    $ udevadm info -a -p /sys/class/ess_class/ess-device-name
    // create udev rules file
    $ cat /etc/udev/rules.d/99-ess-device-name.rules 
        # Rules for ess-device-name
        KERNEL=="ess-device-name", SUBSYSTEM=="ess_class", MODE="0666"

    // target module log realtime monitoring
    $ dmesg -wH

    load syntax :
        $ sudo insmod ess_canonical.ko
        $ sudo insmod ess_canonical.ko module_string="test" module_int_val=5 module_intarray=5,10
        // verify rw access
        $ ls -l /dev/ess-device-name
            crw-rw-rw- 1 root root 240, 0 Sep 23 19:57 /dev/ess-device-name
        $ ./gpio_test
        $ sudo rmmod

    references :
        https://www.kernel.org/doc/html/v4.19/
*/

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>

#include "ess_canonical_module.h"
#include "util.h"

MODULE_AUTHOR("Developer Name <developer_email>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OLED driver");
MODULE_VERSION("0.1");

#include "i2c_oled.h"

/* driver parameters */
#define NUM_DEVICES             1
#define FIRST_REQUESTED_MINOR   0
#define ESS_DEVICE_NAME         "ess-oled"
static struct class* ess_class;
static struct cdev ess_cdev;
static dev_t ess_dev_no;

/* default module parameters */
static char* module_string = "default module string";
static int module_int_val = 2;
static int module_intarray[2] = {0, 1};
module_param(module_string, charp, S_IRUGO); // read
MODULE_PARM_DESC(module_string, "parameter of string type");
module_param(module_int_val, int, S_IRUGO); // read
MODULE_PARM_DESC(module_int_val, "parameter of int type");
module_param_array(module_intarray, int, NULL, S_IWUSR|S_IRUSR); // read/write
MODULE_PARM_DESC(module_intarray, "parameter of int array type");


/* log module parameters */
static void log_parameters(void)
{
    PR_INFO("module_string: %s", module_string);
    PR_INFO("module_int_val: %d", module_int_val);
    PR_INFO("module_intarray: %d, %d", module_intarray[0], module_intarray[1]);

    return;
}

int ess_open(struct inode *i, struct file *f)
{
    PR_INFO("entry");
    return 0;
}

int ess_close(struct inode *i, struct file *f)
{
    PR_INFO("entry()");
    return 0;
}

ssize_t ess_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    PR_INFO("entry()");
    return ess_oled_read(f, buff, count, pos);
}

ssize_t ess_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
{
    PR_INFO("entry()");
    return ess_oled_write(f, buff, count, pos);
}

#if defined(HAVE_UNLOCKED_IOCTL)
static long ess_unlocked_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    PR_INFO("entry");
    // return gpio_irq_demo_ioctl(f, cmd, arg);
    return 0;
}
#else
static int ess_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
{
    return -ENOTTY;;
}
#endif

#if defined(HAVE_COMPAT_IOCTL)
static long ess_compat_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    PR_INFO("entry");
    return 0;
}
#endif

/* support for select(), poll() and epoll() system calls */
__poll_t ess_poll(struct file *f, struct poll_table_struct *wait)
{
    PR_INFO("entry");
    // return gpio_irq_demo_poll(f, wait);
    return 0;
}

// see include/linux/fs.h for full fops description
static struct file_operations ess_fops =
{
  .owner = THIS_MODULE,
  .open = ess_open,
  .release = ess_close,
  .read = ess_read,
  .write = ess_write,
//#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
#if defined(HAVE_UNLOCKED_IOCTL)
    .unlocked_ioctl = ess_unlocked_ioctl,
#else
    .ioctl = ess_ioctl,
#endif
#if defined(HAVE_COMPAT_IOCTL)
    .compat_ioctl = ess_compat_ioctl,
#endif
    .poll = ess_poll
};

/* module load 
    for built-ins this code is placed in a mem section which is freed after init is complete
*/
static int __init
canonical_init(void)
{
    int ret;
    int major;

    PR_INFO("entry");

// TODO handle rets
    log_parameters();

    // gpio_irq_demo_init();
    ess_oled_init();

    if (0 > (ret = alloc_chrdev_region(&ess_dev_no, FIRST_REQUESTED_MINOR, NUM_DEVICES, "ess_region"))) {
        PR_ERR("alloc_chrdev_region() failure");
        return ret;
    }

    /* request /sys/class entry */
    if (NULL == (ess_class = class_create(THIS_MODULE, "ess_class"))) {
        PR_ERR("class_create() failure");
        return -1;
    }

    /* device node */
    if (device_create(ess_class, NULL, ess_dev_no, NULL, ESS_DEVICE_NAME) == NULL) {
        class_destroy(ess_class);
        unregister_chrdev_region(ess_dev_no, 1);
        return -1;
    }

    cdev_init(&ess_cdev, &ess_fops);
    ess_cdev.owner = THIS_MODULE;

    if (cdev_add(&ess_cdev, ess_dev_no, 1) == -1) {
        device_destroy(ess_class, ess_dev_no);
        class_destroy(ess_class);
        unregister_chrdev_region(ess_dev_no, 1);
        return -1;
    }

    major = MAJOR(ess_dev_no);
    PR_INFO("major number : %d", major);

    return 0;
}


/* module unload for non-built ins */
static void __exit
canonical_exit(void)
{
    PR_INFO("entry");

    // gpio_irq_exit();
    ess_oled_cleanup();

    /* driver teardown */
    PR_INFO("release driver resources");
    device_destroy(ess_class, ess_dev_no);
    class_destroy(ess_class);
    unregister_chrdev_region(ess_dev_no, NUM_DEVICES);
}


module_init(canonical_init);
module_exit(canonical_exit);
