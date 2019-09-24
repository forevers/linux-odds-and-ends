#include <linux/delay.h> 
#include <linux/gpio.h>
#include <linux/interrupt.h>

// poll event reads
static DECLARE_WAIT_QUEUE_HEAD(ess_read_queue_);
static bool new_data_ready_ = false;

#include "gpio_irq.h"
#include "gpio_irq_global.h"

/* This module uses the legacy interface for accessing and controlling the gpios.

   Rising edge triggered GPIO interrupt issuing kernal log message during IRQ.
   1.1 kohm R between the GPIO 27 input IRQ and driving GPO.
   Use /sys/class/gpio user space gpio configuration to toggle output pin

     example to drive gp output pin 25 from userspace

       // load module
       $ sudo insmod ess_canonical.ko

       // userspace gpo config
       $ cd /sys/class/gpio/
       $ echo 25 > export
       $ cd gpio25
       // inspect module gpio configuraiton
       $ sudo cat /sys/kernel/debug/gpio
       $ ls
       $ echo out > direction
       $ echo 1 > value
       $ echo 0 > value

       // unload module
       $ sudo rmmod ess_canonical

       // log inspection
       $ dmesg | tail // for end of dmesg
       $ dmesg -wH    // for streaming dmesg from another ssh connection
*/

static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs);

static unsigned int gpio_num_irq_input_ = 27;
static unsigned int gpio_num_output_ = 24;
static unsigned int irq_number_;
static unsigned int num_irqs_ = 0;


int gpio_irq_demo_init(void)
{
    int result;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    if (0 == (result = gpio_request(gpio_num_irq_input_, "gpio_irq_input_label"))) {
        if (0 == (result = gpio_direction_input(gpio_num_irq_input_))) {
            /* TODO - does rpi not impl gpio_set_debounce() ? */
            //if (0 == (result = gpio_set_debounce(gpio_num_irq_input_, 200))) { // msec
            // } else {
            //     pr_err("gpio_set_debounce() failure: %d\n", result);
            // }
            /* allow presentation of gpio in sysfs, but do not allow userspace apps to control pin direction 
                  i.e. $ sudo cat /sys/kernel/debug/gpio
            */
            gpio_export(gpio_num_irq_input_, false);
            if (0 <= (irq_number_ = gpio_to_irq(gpio_num_irq_input_))) {
                if (0 == (result = request_irq(irq_number_, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_irq_handler", NULL))) {

                    // configure gpio output
                    if (0 == (result = gpio_request(gpio_num_output_, "gpio_output_label"))) {
                        if (0 == (result = gpio_direction_output(gpio_num_output_, 0))) {
                            int can_sleep;
                            gpio_export(gpio_num_output_, false);
                            /* TODO 
                               prink(KERN_INFO, "gpio output configuration success\n");
                               $ cat /proc/sys/kernel/printk yeilds 3 as current level ... need 8 for info
                               $ echo (and sudo echo) 8 > /proc/sys/kernel/printk don't work on rpi*/
                            pr_info("gpio output configuration success\n");
                            // prink(KERN_INFO, "gpio output configuration success\n");

                            can_sleep = gpio_cansleep(gpio_num_output_);
                            pr_info("gpio_cansleep(%x) = %d\n", gpio_num_output_, can_sleep);
                            if (1 == can_sleep) {
                                gpio_set_value_cansleep(gpio_num_output_, 1);
                                mdelay(10);
                                gpio_set_value_cansleep(gpio_num_output_, 0);
                                mdelay(10);
                                gpio_set_value_cansleep(gpio_num_output_, 1);
                            } else {
                                gpio_set_value(gpio_num_output_, 1);
                                mdelay(10);
                                gpio_set_value(gpio_num_output_, 0);
                                mdelay(10);
                                gpio_set_value(gpio_num_output_, 1);
                            }

                        } else {
                            gpio_free(gpio_num_output_);
                            pr_err("gpio_direction_output() failure: %d\n", result);
                        }
                    } else {
                        pr_err("gpio_request() failure: %d\n", result);
                    }

                    pr_err("irq configuration success\n");
                } else {
                    pr_err("request_irq() failure: %d\n", result);
                }
            } else {
                pr_err("gpio_to_irq() failure: %d\n", irq_number_);
            }

        } else {
            pr_err("gpio_direction_input() failure: %d\n", result);
            gpio_free(gpio_num_irq_input_);
        }
    } else {
        pr_err("gpio_request() failure: %d\n", result);
    }

    return result;
}


ssize_t gpio_irq_demo_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    if (count) {
        *buff = gpio_get_value(gpio_num_output_);
        return 1;
    } else {
        return 0;
    }
}


ssize_t gpio_irq_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
{
    pr_info("gpio_irq_demo_write()\n");
    pr_info("count = %d\n", count);

    // must be value/delay pair
    if (count % 2) return 0;

    /* write value and msec sleep count */
    pr_info("start gpio sequence\n");
    while (count) {
        pr_info("val = %d\n", (*buff == 0) ? 0 : 1);
        gpio_set_value_cansleep(gpio_num_output_, (*buff == 0) ? 0 : 1);
        buff++;
        mdelay(*buff);
        buff++;
        count -= 2;
    }
    return count;

}


long gpio_irq_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    pr_info("gpio_irq_demo_ioctl()\n");

    switch(cmd) {
        case SET_SEQ_NO:
            pr_info("SET_SEQ_NO\n");
            gpio_set_value_cansleep(gpio_num_output_, 1);
            break;
        case CLR_SEQ_NO:
            pr_info("CLR_SEQ_NO\n");
            gpio_set_value_cansleep(gpio_num_output_, 0);
            break;
        return -ENOTTY;
    }

    return -1;
}


void gpio_irq_exit(void)
{
    pr_info("num_irqs_: %d\n", num_irqs_);
    free_irq(irq_number_, NULL);
    gpio_unexport(gpio_num_irq_input_);
    gpio_free(gpio_num_irq_input_);

    gpio_unexport(gpio_num_output_);
    gpio_free(gpio_num_output_);
}


/* support for select(), poll() and epoll() system calls */
__poll_t gpio_irq_demo_poll(struct file *f, struct poll_table_struct *wait)
{
    unsigned int ret_val_mask = 0;

    pr_info("gpio_irq_demo_poll() : pre\n");
    poll_wait(f, &ess_read_queue_, wait);
    pr_info("gpio_irq_demo_poll() : prost\n");

    if (new_data_ready_) ret_val_mask = POLLIN | POLLRDNORM;

    return ret_val_mask;
}

static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs)
{
    pr_info("gpio_irq_handler()\n");
    pr_info("num_irqs_ = %d\n", ++num_irqs_);

    wake_up_interruptible(&ess_read_queue_);

    return (irq_handler_t) IRQ_HANDLED;
}
