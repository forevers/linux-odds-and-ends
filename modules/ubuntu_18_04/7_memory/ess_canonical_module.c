/* 
    build syntax :
        cross :
            $ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C <path to cross compiled kernel>/linux M=$(pwd) modules
        native :
            $ make -C /lib/modules/$(uname -r)/build M=$(pwd) modules

    // target module log realtime monitoring
    $ dmesg -wH

    load syntax :
        $ sudo insmod ess_memory.ko
        // verify rw access
        $ ls -l /dev/ess-device-name
            crw-rw-rw- 1 root root 240, 0 Sep 23 19:57 /dev/ess-device-name
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

#include "ess_canonical_module.h"
#include "../utils/util.h"

MODULE_AUTHOR("Developer Name <developer_email>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("a canonical driver template");
MODULE_VERSION("0.1");

#include "memory.h"

/* driver parameters */
#define NUM_DEVICES             1
#define FIRST_REQUESTED_MINOR   0
#define ESS_DEVICE_NAME         "ess-device-name"
#define ESS_PROC_NAME           "ess-proc-name"
static struct class* ess_class;
static struct cdev ess_cdev;
static dev_t ess_dev_no;


int ess_open(struct inode *i, struct file *f)
{
    PR_INFO("entry");
    return memory_open();
}


int ess_close(struct inode *i, struct file *f)
{
    PR_INFO("entry()");
    memory_close();
    return 0;
}


int ess_mmap(struct file *filep, struct vm_area_struct *vma)
{
    return memory_mmap(filep, vma);
}


// see include/linux/fs.h for full fops description
static struct file_operations ess_fops =
{
  .owner = THIS_MODULE,
  .open = ess_open,
  .release = ess_close,
  .mmap = ess_mmap,
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

    if (0 > (ret = memory_init(ESS_PROC_NAME))) {
        PR_ERR("memory_init() failure");
        return ret;
    }

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

    memory_exit(ESS_PROC_NAME);

    /* driver teardown */
    PR_INFO("release driver resources");
    device_destroy(ess_class, ess_dev_no);
    class_destroy(ess_class);
    unregister_chrdev_region(ess_dev_no, NUM_DEVICES);
}


module_init(canonical_init);
module_exit(canonical_exit);
