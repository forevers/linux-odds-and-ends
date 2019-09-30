#include <linux/delay.h> 
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>

// poll event reads
static DECLARE_WAIT_QUEUE_HEAD(ess_read_queue_);
static bool new_data_ready_ = false;

#include "gpio_irq.h"
#include "gpio_irq_global.h"

/* This module uses the legacy interface for accessing and controlling the gpios.

   HRT timer availability can bet verified by:
      - CONFIG_HIGH_RES_TIMERS=y in kernel config
      - cat /proc/timer_list and verify nsec resolution reported
      - clock_getres() sys call

   Rising edge triggered GPIO interrupt issuing kernel log message during IRQ.
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

       // also see userspace app built from main.c to toggle through sys calls

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
static int gpio_can_sleep_;


static struct hrt_data {
    struct hrtimer timer;
    ktime_t period;
    long period_sec;
    unsigned long period_nsec;
} *my_hrt_data_;


static enum hrtimer_restart my_kt_function(struct hrtimer *hr_timer)
{
    ktime_t now , interval;
    int gpio_toggle_value = (gpio_get_value(gpio_num_output_) == 0) ? 1 : 0;

    if (1 == gpio_can_sleep_) {
        gpio_set_value_cansleep(gpio_num_output_, gpio_toggle_value);
    } else {
        gpio_set_value(gpio_num_output_, gpio_toggle_value);
    }

    pr_info("time_per_sec : %ld, time_per_nsec : %lu\n", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

    now  = ktime_get();
    interval = ktime_set(my_hrt_data_->period_sec, my_hrt_data_->period_nsec);
    hrtimer_forward(hr_timer, now , interval);

    // HRTIMER_RESTART timer is retarting vs HRTIMER_NORESTART timer is terminating
    return HRTIMER_RESTART;
}


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

                            gpio_export(gpio_num_output_, false);
                            /* TODO 
                               prink(KERN_INFO, "gpio output configuration success\n");
                               $ cat /proc/sys/kernel/printk yeilds 3 as current level ... need 8 for info
                               $ echo (and sudo echo) 8 > /proc/sys/kernel/printk don't work on rpi*/
                            pr_info("gpio output configuration success\n");
                            // prink(KERN_INFO, "gpio output configuration success\n");

                            gpio_can_sleep_ = gpio_cansleep(gpio_num_output_);
                            pr_info("gpio_cansleep(%x) = %d\n", gpio_num_output_, gpio_can_sleep_);
                            if (1 == gpio_can_sleep_) {
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

                            // high res timer
                            my_hrt_data_ = kmalloc(sizeof(*my_hrt_data_), GFP_KERNEL);
                            // CLOCK_REALTIME is network adjusted time vs CLOCK_MONOLITHIC for free running time
                            // HRTIMER_MODE_REL relative time vs HRTIMER_MODE_ABS absolute time
                            pr_info("init the timer\n");
                            hrtimer_init(&my_hrt_data_->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
                            my_hrt_data_->timer.function = my_kt_function;

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
        return -1;
    }
}


ssize_t gpio_irq_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
{
    size_t counts_remaining = count;

    pr_info("gpio_irq_demo_write()\n");
    pr_info("count = %d\n", count);

    // must be value/delay pair
    if (count % 2) return 0;

    // cancel timer if active
    hrtimer_cancel(&my_hrt_data_->timer);

    /* write value and msec sleep count */
    pr_info("start gpio sequence\n");
    while (counts_remaining) {
        pr_info("val = %d\n", (*buff == 0) ? 0 : 1);
        gpio_set_value_cansleep(gpio_num_output_, (*buff == 0) ? 0 : 1);
        buff++;
        mdelay(*buff);
        buff++;
        counts_remaining -= 2;
    }
    return count/2;

}


long gpio_irq_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    pr_info("gpio_irq_demo_ioctl()\n");

    switch(cmd) {
        case ESS_SET_GPIO:
            pr_info("ESS_SET_GPIO_SEQ_NUM\n");
            // cancel timer if active
            hrtimer_cancel(&my_hrt_data_->timer);
            gpio_set_value_cansleep(gpio_num_output_, 1);
            break;

        case ESS_CLR_GPIO:
            pr_info("ESS_CLR_GPIO_SEQ_NUM\n");
            // cancel timer if active
            hrtimer_cancel(&my_hrt_data_->timer);
            gpio_set_value_cansleep(gpio_num_output_, 0);
            break;

        case ESS_DUTY_CYCLE_GPIO:
            pr_info("ESS_DUTY_CYCLE_GPIO_SEQ_NUM\n");
            pr_info("period : %lu msec\n", arg);
            if (arg == 0) {
                if (hrtimer_cancel(&my_hrt_data_->timer)) {
                    pr_info("cancelled active timer\n");
                }
            } else {
                my_hrt_data_->period_sec = arg / 1000;
                my_hrt_data_->period_nsec = (arg - my_hrt_data_->period_sec*1000) *1000000;
                pr_info("time_per_sec : %ld, time_per_nsec : %lu\n", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

                my_hrt_data_->period = ktime_set(my_hrt_data_->period_sec, my_hrt_data_->period_nsec);
                // cancel and start new timer at specified period
                hrtimer_cancel(&my_hrt_data_->timer);
                hrtimer_start(&my_hrt_data_->timer, my_hrt_data_->period, HRTIMER_MODE_REL);
            }
            break;

        default:
            retval = -EPERM;
            break;

        return retval;
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

    // timer resource
    if (hrtimer_cancel(&my_hrt_data_->timer)) {
        pr_info("cancelled active timer\n");
    }
    kfree(my_hrt_data_);
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
