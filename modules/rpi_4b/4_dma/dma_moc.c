#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "../utils/util.h"

/* 
    memory config info:

        boot log usually dumps memory zones
            $ journalctl -b | grep DMA
                kernel:   DMA      [mem 0x0000000000001000-0x0000000000ffffff]
                kernel:   DMA32    [mem 0x0000000001000000-0x00000000ffffffff]
            $ journalctl -b | grep Normal
                kernel:   Normal   [mem 0x0000000100000000-0x000000082fffffff]

        buddyinfo dump frag information
            $ cat /proc/buddyinfo 
                Node 0, zone      DMA      2      1      0      1      3      0      1      0      1      1      3 
                Node 0, zone    DMA32  36761  21504  10512   5206    860     74      1      0      0      0      0 
                Node 0, zone   Normal  50784  43294  29049   9463    425      5      1      1      0      0      0 

        a procs vma collection
            $ cat /proc/<pid>/maps
            $ sudo pmap <pid>

        proc mmio summary
            $ sudo cat /proc/mmio

    references:
        'Linux Device Driver Development with Raspberry PI - Practical Labs'
        https://www.kernel.org/doc/html/latest/driver-api/dmaengine/client.html
        https://www.kernel.org/doc/Documentation/DMA-API-HOWTO.txt
        https://www.kernel.org/doc/Documentation/scheduler/completion.txt

    tested against 32bit kernel install:
        DEPMOD  5.10.52-v7l+
*/

struct dma_private_data {
    char *data_tx_buffer;               /* dma transmit buffer */
    char *data_rx_buffer;               /* dma receive buffer */
    __u32 kmalloc_mmap_buffer_size;     /* dts configured buffer size */
    struct dma_chan *dma_channel;       /* tx -> rx channel */
    struct miscdevice dma_misc_dev;     /* char mode device */
    struct device *pdev;                /* probe provided device */
    struct completion dma_completion;   /* completion better than semaphore sync */
};

#define ESS_DEVICE_NAME         "ess-dma-moc"

/* mmap */
static void* mmap_kmalloc_init_kernel_ = NULL;
static DEFINE_MUTEX(mmap_mutex_);

/* procfs exposes buffer size */
static struct proc_dir_entry *dma_proc_dir_entry_;
#define ESS_PROC_NAME           "ess-proc-name"


int dma_mmap(struct file *filep, struct vm_area_struct *vma)
{
    int ret = 0;
    struct page *page = NULL;
    unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
    unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
    struct dma_private_data *dma_private_data_ref;

    PR_INFO("entry");

    /* encapsulated file device reference */
    dma_private_data_ref = container_of(filep->private_data, struct dma_private_data, dma_misc_dev);

    /* offset test */
    if (offset > dma_private_data_ref->kmalloc_mmap_buffer_size) {
        PR_ERR("requested offset %lx greater than buffer size %x", offset, dma_private_data_ref->kmalloc_mmap_buffer_size);
        return -EINVAL;
    }

    if (size > dma_private_data_ref->kmalloc_mmap_buffer_size - offset) {
        PR_ERR("requested size %lx greater than buffer size %x - offset", size, dma_private_data_ref->kmalloc_mmap_buffer_size);
        return -EINVAL;
    } 
   
    PR_INFO("requested size: %lx", size);
    PR_INFO("requested offset: %lx", offset);
    PR_INFO("available size %x", dma_private_data_ref->kmalloc_mmap_buffer_size);

    page = virt_to_page((unsigned long)mmap_kmalloc_init_kernel_ + offset); 

    if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
        PR_ERR("writeable mappings must be shared, rejecting");
        return(-EINVAL);
    }

    /* no swapping */
    vma->vm_flags |= VM_LOCKED;

    if (remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), size, vma->vm_page_prot)) {
        PR_INFO("exit");
        return -EAGAIN;
    }

    PR_INFO("exit");
    return ret;
}


static void dma_done_callback(void *data)
{
    struct dma_private_data *dma_private_data_ref = data;

    PR_INFO("entry");

    complete(&dma_private_data_ref->dma_completion);

    PR_INFO("exit");
}


ssize_t dma_write(struct file *file, const char __user *user_buf, size_t count, loff_t *pos)
{
    int retval;
    struct dma_private_data *dma_private_data_ref;
    struct dma_device *dma_dev;
    dma_addr_t dma_src_addr;
    dma_addr_t dma_dest_addr;
    struct dma_async_tx_descriptor *dma_descriptor;
    dma_cookie_t dma_cookie;
    char stash_null;

    PR_INFO("entry");
    PR_INFO("dma 0x%x bytes", count);

    /* encapsulated file device reference */
    dma_private_data_ref = container_of(file->private_data, struct dma_private_data, dma_misc_dev);

    /* userspace data -> kalloc'ed virtual buffer, ignore offset */
    count = (count > dma_private_data_ref->kmalloc_mmap_buffer_size) ? dma_private_data_ref->kmalloc_mmap_buffer_size : count;

    if ((retval = copy_from_user(dma_private_data_ref->data_tx_buffer, user_buf, count))) {
        PR_ERR("copy_from_user() failure: %x", retval);
        return -EINVAL;
    }

    PR_INFO("fist transfer byte %x of %d bytes", dma_private_data_ref->data_tx_buffer[0], count);

    /* pair with dma_unmap_single() at end of transaction */
    dma_src_addr = dma_map_single(dma_private_data_ref->pdev,
                        dma_private_data_ref->data_tx_buffer,
                        count,
                        DMA_TO_DEVICE);

    /* pair with dma_unmap_single() at end of transaction */
    dma_dest_addr = dma_map_single(dma_private_data_ref->pdev,
                        dma_private_data_ref->data_rx_buffer,
                        count,
                        DMA_TO_DEVICE);

    /* class device for sysfs */
    dma_dev = dma_private_data_ref->dma_channel->device;

    /* use the memcpy version of device_prep_dma_xxx() */
    dma_descriptor = dma_dev->device_prep_dma_memcpy(dma_private_data_ref->dma_channel, 
                                dma_dest_addr,
                                dma_src_addr,
                                count,
                                DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
    dma_descriptor->callback = dma_done_callback;
    dma_descriptor->callback_param = dma_private_data_ref;

    /* construct completion */
    init_completion(&dma_private_data_ref->dma_completion);

    /* enqueue dma transaction */
    dma_cookie = dmaengine_submit(dma_descriptor);
    if (dma_submit_error(dma_cookie)) {
        PR_ERR("dma submition failure");
        PR_INFO("exit");
        return -EINVAL;
    }

    /* submit transaction */
    dma_async_issue_pending(dma_private_data_ref->dma_channel);
    wait_for_completion(&dma_private_data_ref->dma_completion);
    // TODO don't think this is required
    dma_async_is_tx_complete(dma_private_data_ref->dma_channel, dma_cookie, NULL, NULL);

    stash_null = dma_private_data_ref->data_rx_buffer[16];
    dma_private_data_ref->data_rx_buffer[16] = 0;
    PR_INFO("dma rx buf string %s", dma_private_data_ref->data_rx_buffer);
    PR_INFO("0: %d", dma_private_data_ref->data_rx_buffer[0]);
    PR_INFO("1: %d", dma_private_data_ref->data_rx_buffer[1]);
    PR_INFO("2: %d", dma_private_data_ref->data_rx_buffer[2]);
    dma_private_data_ref->data_rx_buffer[16] = stash_null;

    /* release the dma mappings */ 
    dma_unmap_single(dma_private_data_ref->pdev, dma_src_addr, count, DMA_TO_DEVICE);
    dma_unmap_single(dma_private_data_ref->pdev, dma_dest_addr, count, DMA_TO_DEVICE);

    PR_INFO("exit");
    return count;
}

/* simple dma dev fops */
struct file_operations dma_fops = {
    .owner = THIS_MODULE,
    .write = dma_write,
    // .release = dma_close,
    // .mmap = dma_mmap,
};


static ssize_t proc_memory_read(struct file *file, char __user *ubuf,size_t count, loff_t *ppos) 
{
    // TODO use more recent proc file generation framework
    char buf[256];
    int len=0;
    struct dma_private_data *dma_private_data_ref = container_of(file->private_data, struct dma_private_data, dma_misc_dev);

    PR_INFO("entry");
    if(*ppos > 0 || count < 256)
        return 0;

    len += sprintf(buf, "size = %d\n", dma_private_data_ref->kmalloc_mmap_buffer_size);
    len += sprintf(buf + len, "blaa = ...\n");

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;
    *ppos = len;

    return len;
}


/* simple RO proc file */
/* 32 bit linux api */
static const struct proc_ops dma_proc_file_fops = {
	// .proc_open	= alignment_proc_open,
	.proc_read	= proc_memory_read,
	// .proc_lseek	= seq_lseek,
	// .proc_release	= single_release,
	// .proc_write	= alignment_proc_write,
};


/* create procfs entry */
static int create_proc(char* proc_name)
{
    PR_INFO("entry");
    PR_INFO("exit");

    /* create /proc/<proc_name> file */

    /* 32 bit linux api */
    if (NULL == (dma_proc_dir_entry_ = proc_create(proc_name, 0, NULL, &dma_proc_file_fops))) return -ENOMEM;

    return 0;
}


/* remove procfs entry */
static void dma_remove_proc(void)
{
    PR_INFO("entry");
    proc_remove(dma_proc_dir_entry_);
    PR_INFO("exit");
}


/* module init */
static int dma_probe(struct platform_device *pdev)
{
    struct dma_private_data *dma_private_data_ref;
    dma_cap_mask_t dma_capabilty_mask;
    struct device_node *node;
    int retval;

    PR_INFO("entry");

    PR_INFO("PAGE_SIZE: %ld", PAGE_SIZE);
    PR_INFO("process kernel vma start address PAGE_SIZE: %lx", PAGE_OFFSET);

    /* dma_private_data_ref released automatically during driver release, is contiguos virtual and physical */
    if (!(dma_private_data_ref = devm_kzalloc(&pdev->dev, sizeof(struct dma_private_data), GFP_KERNEL))) return -ENOMEM;
    PR_INFO("dma_device: %p", dma_private_data_ref);
    dma_private_data_ref->dma_misc_dev.minor = MISC_DYNAMIC_MINOR;
    dma_private_data_ref->dma_misc_dev.name = ESS_DEVICE_NAME;
    dma_private_data_ref->dma_misc_dev.fops = &dma_fops;
    dma_private_data_ref->dma_misc_dev.mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    dma_private_data_ref->pdev = &pdev->dev;

    /* configure buffer size from dts */
    if ((node = of_find_node_by_name(NULL, "ess-dma-moc"))) {
        if (!of_property_read_u32(node, "buffer_size", &dma_private_data_ref->kmalloc_mmap_buffer_size)) {
            PR_INFO("buffer_size: %d", dma_private_data_ref->kmalloc_mmap_buffer_size);
        } else {
            PR_ERR("of_property_read_u32() failure");
        }
    } else {
        PR_ERR("of_find_node_by_name() failure");
    }

    /* allocation are mod PAGE_SIZE ... will round up ... contiguous in virtual and physical memory*/
    /* data_tx_buffer released automatically during driver release, is contiguos virtual and physical */
    if (!(dma_private_data_ref->data_tx_buffer = devm_kzalloc(&pdev->dev, dma_private_data_ref->kmalloc_mmap_buffer_size, GFP_KERNEL))) {
         PR_ERR("devm_kzalloc() failure");
         return -ENOMEM;
    }
    PR_INFO("data_tx_buffer: %p", dma_private_data_ref->data_tx_buffer);
    
     /* data_rx_buffer released automatically during driver release, is contiguos virtual and physical */
    if (!(dma_private_data_ref->data_rx_buffer = devm_kzalloc(&pdev->dev, dma_private_data_ref->kmalloc_mmap_buffer_size, GFP_KERNEL))) {
        PR_ERR("devm_kzalloc() failure");
        return -ENOMEM;
    }
    PR_INFO("data_rx_buffer: %p", dma_private_data_ref->data_rx_buffer);

    /* allocate a DMA slave channel for memory to memory copy */
    dma_cap_zero(dma_capabilty_mask);
    dma_cap_set(DMA_MEMCPY, dma_capabilty_mask);
    if (!(dma_private_data_ref->dma_channel = dma_request_channel(dma_capabilty_mask, 0, NULL))) {
        PR_ERR("dma_request_channel() failure");
        return -EINVAL;
    }

    /* register misc device */
    if ((retval = misc_register(&dma_private_data_ref->dma_misc_dev))) {
        PR_ERR("dma_request_channel() failure");
    }

    /* platform framework */
    platform_set_drvdata(pdev, dma_private_data_ref);

    PR_INFO("exit");
    return 0;
}


int dma_remove(struct platform_device *pdev)
{
    struct dma_private_data *dma_private_data_ref;

    PR_INFO("entry");

    // TODO don't think this one required
    mutex_unlock(&mmap_mutex_);

    PR_INFO("unregister misc device and release dma channel");
    dma_private_data_ref = platform_get_drvdata(pdev);
    misc_deregister(&dma_private_data_ref->dma_misc_dev);
    dma_release_channel(dma_private_data_ref->dma_channel);

    PR_INFO("exit");
    return 0;
}


int dma_open(struct inode *i, struct file *f)
{
    PR_INFO("entry");

    if (!mutex_trylock(&mmap_mutex_)) {
        PR_ERR("mutex_trylock() failure");
        return -EBUSY;
    }

    PR_INFO("exit");
    return 0;
}


int dma_close(struct inode *i, struct file *f)
{
    PR_INFO("entry");
    mutex_unlock(&mmap_mutex_);
    return 0;
}


/* Open Firmware matching */
static const struct of_device_id ess_dma_of_id[] = {
    {.compatible = "ess,ess-dma-moc"},
    {}
};
MODULE_DEVICE_TABLE(of, ess_dma_of_id);
/*
    pi@raspberrypi:~ $ cat /proc/device-tree/soc/ess-dma-moc/compatible 
    ess,ess-dma-moc
*/


static struct platform_driver dma_test_platform_driver = {
    .probe = dma_probe,
    .remove = dma_remove,
    .driver = {
        .name = "ess-dma-moc",
        .of_match_table = ess_dma_of_id,
        .owner = THIS_MODULE,
    },
};


/* module load 
    for built-ins this code is placed in a mem section which is freed after init is complete
*/
static int __init dma_init(void)
{
    int ret;
    int num_cores = num_online_cpus();

    PR_INFO("entry");
    PR_INFO("%d cpus", num_cores);

    if ((ret = platform_driver_register(&dma_test_platform_driver))) {
        PR_ERR("platform_driver_register() failure: %x", ret);
        return ret;
    }

    create_proc(ESS_PROC_NAME);

    PR_INFO("exit");
    return 0;
}


/* module unload for non-built ins */
static void __exit dma_exit(void)
{
    PR_INFO("entry");

    dma_remove_proc();

    /* driver teardown */
    PR_INFO("unregister misc driver");
    platform_driver_unregister(&dma_test_platform_driver);
    PR_INFO("exit");
}


module_init(dma_init);
module_exit(dma_exit);

MODULE_AUTHOR("Developer Name <developer_email>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("a canonical misc driver template");
MODULE_VERSION("0.1");