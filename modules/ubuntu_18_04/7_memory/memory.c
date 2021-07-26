#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

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

        proc vma summary
            $ sudo cat /proc/vmallocinfo

        proc mmio summary
            $ sudo cat /proc/mmio

    references:
        https://www.kernel.org/doc/html/latest/core-api/memory-allocation.html
        <linux/gfp.h>
        https://linux-kernel-labs.github.io/refs/heads/master/labs/memory_mapping.html
        https://www.kernel.org/doc/html/latest/arm64/memory.html
        http://thinkiii.blogspot.com/2014/02/arm64-linux-kernel-virtual-address-space.html
        https://developer.arm.com/documentation/101811/0101/Address-spaces-in-AArch64
        https://www.kernel.org/doc/Documentation/vm/unevictable-lru.txt
*/


/* any size requested, but allocation is mod PAGE_SIZE */
static void* data_init_kernel_ = NULL;
static void* data_init_dma_ = NULL;
static void* data_init_user_ = NULL;
static void* data_init_user_hi_ = NULL;

/* direct page allocation */
static unsigned long data_init_page_kernel_;
static unsigned long data_init_page_zeroed_kernel_ ;
static unsigned long data_init_pages_kernel_;
static unsigned long data_init_pages_dma_;

/* vmalloc allocation */
static void* vmalloc_mem_ = NULL;
static void* vzalloc_mem_ = NULL;

/* mmap */
#define KMALLOC_MMAP_PAGES 16
#define KMALLOC_MMP_BUFFER_SIZE (KMALLOC_MMAP_PAGES*PAGE_SIZE)
#define ALLOC_PAGES_MMAP_PAGES 8
static void* mmap_kmalloc_init_kernel_ = NULL;
// TODO impl pages version ... static unsigned long mmap_alloc_pages_init_kernel_;
static DEFINE_MUTEX(mmap_mutex_);
static struct proc_dir_entry * memory_proc_dir_entry_;


static ssize_t proc_memory_read(struct file *file, char __user *ubuf,size_t count, loff_t *ppos) 
{
    // TODO use more recent proc file generation framework
    char buf[256];
    int len=0;
    PR_INFO("entry");
    if(*ppos > 0 || count < 256)
        return 0;
    len += sprintf(buf, "size = %ld\n", KMALLOC_MMP_BUFFER_SIZE);
    len += sprintf(buf + len, "blaa = ...\n");

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;
    *ppos = len;

    return len;
}


/* simple RO proc file */
static const struct file_operations proc_file_fops = {
    .owner = THIS_MODULE,
    .read = proc_memory_read,
};


/* create procfs entry */
static int create_proc(char* proc_name)
{
    PR_INFO("entry");
    PR_INFO("exit");

    /* create /proc/<proc_name> file */
    if (NULL == (memory_proc_dir_entry_ = proc_create(proc_name, 0, NULL, &proc_file_fops))) return -ENOMEM;

    return 0;
}


/* remove procfs entry */
static void memory_remove_proc(void)
{
    PR_INFO("entry");
    proc_remove(memory_proc_dir_entry_);
    PR_INFO("exit");
}


static void free_memory(void)
{
    kfree(mmap_kmalloc_init_kernel_);

    free_page(data_init_page_kernel_);
    free_page(data_init_page_zeroed_kernel_);
    {
        unsigned long order = 3; // 2^3 = 8 pages requested
        free_pages(data_init_pages_kernel_, order);
        free_pages(data_init_pages_dma_, order);
    }

    kfree(data_init_kernel_);
    kzfree(data_init_dma_);
    kfree(data_init_user_);
    kzfree(data_init_user_hi_);

    vfree(vmalloc_mem_);
    vfree(vzalloc_mem_);
}


/* module init */
int memory_init(char* proc_name)
{
    // int i;
    int num_cores = num_online_cpus();

    PR_INFO("entry");
    PR_INFO("%d cpus", num_cores);

    PR_INFO("PAGE_SIZE: %ld", PAGE_SIZE);
    PR_INFO("process kernel vma start address PAGE_SIZE: %lx", PAGE_OFFSET);

    /* allocation are mod PAGE_SIZE ... will round up ... contiguous in virtual and physical memory*/
    /* kmalloc is contiguos virtual and physical */
    if (!(data_init_kernel_ = kmalloc(PAGE_SIZE, GFP_KERNEL))) goto err;
    PR_INFO("data_init_kernel_: %p", data_init_kernel_);
    if (!(data_init_kernel_ = krealloc(data_init_kernel_, 2*PAGE_SIZE, GFP_KERNEL))) goto err;
    PR_INFO("data_init_kernel_: %p", data_init_kernel_);
    if (!(data_init_dma_ = kzalloc(PAGE_SIZE, GFP_DMA))) goto err;
    PR_INFO("data_init_dma_: %p", data_init_dma_);
    if (!(data_init_user_ = kmalloc(PAGE_SIZE, GFP_USER))) goto err;
    PR_INFO("data_init_user_: %p", data_init_user_);
    if (!(data_init_user_hi_ = kzalloc(PAGE_SIZE, GFP_HIGHUSER))) goto err;
    PR_INFO("data_init_user_hi_: %p", data_init_user_hi_);

    /* page level allocation */
    data_init_page_kernel_ = __get_free_page(GFP_KERNEL);
    data_init_page_zeroed_kernel_ = get_zeroed_page(GFP_KERNEL);
    {
        unsigned long order = 3; // 2^3 = 8 pages requested
        data_init_pages_kernel_ = __get_free_pages(GFP_KERNEL, order);
        data_init_pages_dma_ = __get_dma_pages(GFP_KERNEL, order);
    }
    {
        unsigned long order = 3; // 2^3 = 8 pages requested
        struct page* temp_page = alloc_pages(GFP_KERNEL, order);
        PR_INFO("page address: %p", page_to_virt(temp_page));
        PR_INFO("page address: %p", page_address(temp_page));

        __free_pages(temp_page, order);
    }

    /* vmalloc for contigious virtual / fragmented physical */
    if (!(vmalloc_mem_ = vmalloc(PAGE_SIZE - 16))) goto err;
    if (!(vzalloc_mem_ =  vzalloc(PAGE_SIZE - 16))) goto err;

    /* mmap */
    if (!(mmap_kmalloc_init_kernel_ = kmalloc(KMALLOC_MMP_BUFFER_SIZE, GFP_KERNEL))) goto err;
    PR_INFO("mmap_kmalloc_init_kernel_: %p", mmap_kmalloc_init_kernel_);

    create_proc(proc_name);

    return 0;

    err: 
    free_memory();
    return -ENOMEM;;

}


/* module exit */
void memory_exit(char* proc_name)
{
    PR_INFO("entry");

    memory_remove_proc();

    mutex_unlock(&mmap_mutex_);

    free_memory();

    PR_INFO("exit");
}


int memory_open(void)
{
    PR_INFO("entry");

    if (!mutex_trylock(&mmap_mutex_)) {
        PR_ERR("mutex_trylock() failure");
        return -EBUSY;
    }

    PR_INFO("exit");
    return 0;
}


void memory_close(void)
{
    PR_INFO("entry");
    mutex_unlock(&mmap_mutex_);
    PR_INFO("exit");
}


int memory_mmap(struct file *filep, struct vm_area_struct *vma)
{
    int ret = 0;
    struct page *page = NULL;
    unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
    unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;

    PR_INFO("entry");

    /* offset test */
    if (offset > KMALLOC_MMP_BUFFER_SIZE) {
        PR_ERR("requested offset %lx greater than buffer size %lx", offset, KMALLOC_MMP_BUFFER_SIZE);
        return -EINVAL;
    }

    if (size > KMALLOC_MMP_BUFFER_SIZE - offset) {
        PR_ERR("requested size %lx greater than buffer size  %lx - offset", size, KMALLOC_MMP_BUFFER_SIZE);
        return -EINVAL;
    } 
   
    PR_INFO("requested size: %lx", size);
    PR_INFO("requested offset: %lx", offset);
    PR_INFO("available size %lx", KMALLOC_MMP_BUFFER_SIZE);

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
