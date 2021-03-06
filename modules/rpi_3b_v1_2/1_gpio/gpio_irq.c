#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "circ_buffer_mod2_elements.h"
#include "gpio_irq.h"
#include "gpio_irq_global.h"
#include "util.h"

/* This module uses the legacy interface for accessing and controlling the gpios.

    HRT timer availability can bet verified by:
      - CONFIG_HIGH_RES_TIMERS=y in kernel config
      - cat /proc/timer_list and verify nsec resolution reported
      - clock_getres() sys call

    Rising edge triggered GPIO interrupt issuing kernel log message during IRQ.
    1.1 kohm R between the GPIO 27 input IRQ and driving GPO.
    Use /sys/class/gpio user space gpio configuration to toggle output pin

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
        // unexport when complete
        $ echo 25 > unexport

        // query interrupt status
        $ cat /proc/interrupts

        // also see userspace app built from main.c to toggle through sys calls

        // unload module
        $ sudo rmmod ess_canonical

        // log inspection
        $ dmesg | tail // for end of dmesg
        $ dmesg -wH    // for streaming dmesg from another ssh connection
*/

static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs);
static void ess_do_work(struct work_struct* work);

static unsigned int gpio_num_irq_input_ = 27;
static unsigned int gpio_num_output_ = 24;
static unsigned int irq_number_;
static unsigned int num_irqs_ = 0;
static int gpio_can_sleep_;

/* high resolution timer used to drive gpo pwm */
static struct HrtData {
    struct hrtimer timer;
    ktime_t period;
    long period_sec;
    unsigned long period_nsec;
} *my_hrt_data_;

/* work queue for bottom half interrupt processing */
// TODO create custom ess workqueue */
struct ESSWorkStructWrapper {
    struct work_struct event_work_struct;
    struct CircularBufferMod2 capture_event_buffer;     /* upper half irq data */
    spinlock_t capture_event_spinlock;                  /* capture_event_buffer shared between irq and workqueue (or tasklet TODO) */
    bool poll_enabled;                                  /* device supports polling */ 
    wait_queue_head_t capture_event_waitqueue;          /* irq lower half processing by workqueue (or tasklet TODO) */
    uint64_t event;                                     /* incrementing event number */
    struct CircularBufferMod2 event_bulk_data_buffer;   /* lower half data */
    struct mutex event_bulk_data_mtx;                   /* event_bulk_data_buffer access mutex */
} ess_work_struct_wrapper_;


static enum hrtimer_restart my_kt_function(struct hrtimer *hr_timer)
{
    ktime_t now , interval;
    int gpio_toggle_value = (gpio_get_value(gpio_num_output_) == 0) ? 1 : 0;

    if (1 == gpio_can_sleep_) {
        gpio_set_value_cansleep(gpio_num_output_, gpio_toggle_value);
    } else {
        gpio_set_value(gpio_num_output_, gpio_toggle_value);
    }

    PR_INFO("time_per_sec : %ld, time_per_nsec : %lu", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

    now  = ktime_get();
    interval = ktime_set(my_hrt_data_->period_sec, my_hrt_data_->period_nsec);
    hrtimer_forward(hr_timer, now , interval);

    // HRTIMER_RESTART timer is retarting vs HRTIMER_NORESTART timer is terminating
    return HRTIMER_RESTART;
}


int gpio_irq_demo_init(void)
{
    int result = -1;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    // TODO consider to moving much of this to the open() call to allow potential HW sharing
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

                // TODO pass device object to driver in last parameter
                if (0 == (result = request_irq(irq_number_, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_irq_handler", NULL))) {

                    if (0 == (result = init(&ess_work_struct_wrapper_.capture_event_buffer, sizeof(struct CaptureEvent), MAX_MOD_2_BUFFER_ELEMENTS(CaptureEvent)))) {

                        if (0 == (result = init(&ess_work_struct_wrapper_.event_bulk_data_buffer, sizeof(struct EventBulkData), MAX_MOD_2_BUFFER_ELEMENTS(EventBulkData)))) {

                            ess_work_struct_wrapper_.event = 0;
                            ess_work_struct_wrapper_.poll_enabled = false;

                            /* irq reads fill capture_event_buffer */
                            init_waitqueue_head(&ess_work_struct_wrapper_.capture_event_waitqueue);

                            INIT_WORK(&ess_work_struct_wrapper_.event_work_struct, ess_do_work);
                            spin_lock_init(&ess_work_struct_wrapper_.capture_event_spinlock);

                            // sema_init(&(ess_work_struct_wrapper_->sem), 1);
                            mutex_init(&ess_work_struct_wrapper_.event_bulk_data_mtx);

                            // configure gpio output
                            if (0 == (result = gpio_request(gpio_num_output_, "gpio_output_label"))) {

                                if (0 == (result = gpio_direction_output(gpio_num_output_, 0))) {

                                    gpio_export(gpio_num_output_, false);
                                    /* TODO 
                                       prink(KERN_INFO, "gpio output configuration success\n");
                                       $ cat /proc/sys/kernel/printk yeilds 3 as current level ... need 8 for info
                                       $ echo (and sudo echo) 8 > /proc/sys/kernel/printk don't work on rpi*/
                                    PR_INFO("gpio output configuration success");

                                    gpio_can_sleep_ = gpio_cansleep(gpio_num_output_);
                                    PR_INFO("gpio_cansleep(%x) = %d", gpio_num_output_, gpio_can_sleep_);
#if 0
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
#endif
                                    // high res timer
                                    my_hrt_data_ = kmalloc(sizeof(*my_hrt_data_), GFP_KERNEL);
                                    // CLOCK_REALTIME is network adjusted time vs CLOCK_MONOLITHIC for free running time
                                    // HRTIMER_MODE_REL relative time vs HRTIMER_MODE_ABS absolute time
                                    PR_INFO("init the timer");
                                    hrtimer_init(&my_hrt_data_->timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
                                    my_hrt_data_->timer.function = my_kt_function;

                                } else {
                                    gpio_free(gpio_num_output_);
                                    PR_ERR("gpio_direction_output() failure: %d", result);
                                }
                            } else {
                                PR_ERR("gpio_request() failure: %d", result);
                            }
                        } else {
                            PR_ERR("init() EventBulkData failure: %d", result);
                        }                   
                    } else {
                        PR_ERR("init() CaptureEvent failure: %d", result);
                    }
                } else {
                    PR_ERR("request_irq() failure: %d", result);
                }
            } else {
                PR_ERR("gpio_to_irq() failure: %d", irq_number_);
            }
        } else {
            PR_ERR("gpio_direction_input() failure: %d", result);
            gpio_free(gpio_num_irq_input_);
        }
    } else {
        PR_ERR("gpio_request() failure: %d", result);
    }

    return result;
}

/* when device is in ESS_DUTY_CYCLE_GPIO mode, the read pulls from a buffer */
ssize_t gpio_irq_demo_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    ssize_t bytes_read = 0;

    PR_INFO("entry");

    PR_INFO("count passed: %zu, expect mod of %u", count, sizeof(struct EventBulkData));

    if (1 == count) {

        PR_INFO("read current value");

        /* read current value */
        *buff = gpio_get_value(gpio_num_output_);
        return 1;

    } else if ((count % sizeof(struct EventBulkData)) == 0) {

        struct EventBulkData* event_bulk_data;
        struct CircularBufferMod2* event_bulk_data_buffer;

        PR_INFO("read oldest event");

        event_bulk_data_buffer = &ess_work_struct_wrapper_.event_bulk_data_buffer;

        /* read from event_bulk_data_buffer */
        mutex_lock(&ess_work_struct_wrapper_.event_bulk_data_mtx);

        while ( (count >= sizeof(struct EventBulkData)) && space(event_bulk_data_buffer)) {

            count -= sizeof(struct EventBulkData);

            if (NULL != (event_bulk_data = front(event_bulk_data_buffer))) {

                if (!copy_to_user(buff, event_bulk_data, sizeof(struct EventBulkData))) {
                    buff += sizeof(struct EventBulkData);
                    bytes_read += sizeof(struct EventBulkData);
                    pop(event_bulk_data_buffer);
                } else {
                    PR_ERR("copy_to_user() failure");
                    bytes_read = -EPERM;
                    break;
                }
            } else {
                PR_ERR("front() failure");
                bytes_read = -EPERM;
                break;
            }
        }

        mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);

    } else {
        /* invalid read request size*/
        PR_ERR("invalid read request size");
        bytes_read = -EPERM;
    }

    return bytes_read;
}


ssize_t gpio_irq_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
{
    size_t counts_remaining = count;

    PR_INFO("entry");
    PR_INFO("count = %d", count);

    /* must be value/delay pair */
    if (count % 2) return (ssize_t)0;

    /* cancel timer if active */
    hrtimer_cancel(&my_hrt_data_->timer);

    /* write value and msec sleep count */
    PR_INFO("start gpio sequence");
    while (counts_remaining) {
        PR_INFO("val = %d", (*buff == 0) ? 0 : 1);
        gpio_set_value_cansleep(gpio_num_output_, (*buff == 0) ? 0 : 1);
        buff++;
        mdelay(*buff);
        buff++;
        counts_remaining -= 2;
    }

    return (ssize_t)(count/2);
}


long gpio_irq_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    PR_INFO("gpio_irq_demo_ioctl()");

    switch(cmd) {

        case ESS_SET_GPIO:

            PR_INFO("ESS_SET_GPIO_SEQ_NUM");
            // cancel timer if active
            hrtimer_cancel(&my_hrt_data_->timer);

            /* set gpo value */
            gpio_set_value_cansleep(gpio_num_output_, 1);

            // TODO have a statefull behavior for handling ESS_DUTY_CYCLE_GPIO mode vs set/get mode
            /* signal any blocked poll (select(), poll() or epoll() sys call) */
            ess_work_struct_wrapper_.poll_enabled = false;
            wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));

            break;

        case ESS_CLR_GPIO:

            PR_INFO("ESS_CLR_GPIO_SEQ_NUM");
            // cancel timer if active
            hrtimer_cancel(&my_hrt_data_->timer);

            /* clear gpo */
            gpio_set_value_cansleep(gpio_num_output_, 0);

            // TODO have a statefull behavior for handling ESS_DUTY_CYCLE_GPIO mode vs set/get mode ... and sync the release of the user space call
            /* signal any blocked poll (select(), poll() or epoll() sys call) */
            ess_work_struct_wrapper_.poll_enabled = false;
            wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));
            break;

        case ESS_DUTY_CYCLE_GPIO:

            PR_INFO("ESS_DUTY_CYCLE_GPIO_SEQ_NUM");
            PR_INFO("period : %lu msec", arg);

            if (arg == 0) {

                PR_INFO("cancel duty cycle mode");
                if (hrtimer_cancel(&my_hrt_data_->timer)) {
                    pr_info("cancelled active timer\n");
                }

                ess_work_struct_wrapper_.poll_enabled = false;
                wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));

            } else {

                my_hrt_data_->period_sec = arg / 1000;
                my_hrt_data_->period_nsec = (arg - my_hrt_data_->period_sec*1000) *1000000;
                PR_INFO("time_per_sec : %ld, time_per_nsec : %lu", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

                ess_work_struct_wrapper_.poll_enabled = true;

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

    return retval;
}


void gpio_irq_exit(void)
{
    // TODO consider to moving much of this to the close() call to allow potential HW sharing

    PR_INFO("entry");
    PR_INFO("num_irqs_: %d", num_irqs_);
    free_irq(irq_number_, NULL);
    gpio_unexport(gpio_num_irq_input_);
    gpio_free(gpio_num_irq_input_);

    gpio_unexport(gpio_num_output_);
    gpio_free(gpio_num_output_);

    /* timer resources */
    PR_INFO("cancel active time");
    if (hrtimer_cancel(&my_hrt_data_->timer)) {
        PR_INFO("cancelled active timer");
    }
    PR_INFO("free timer object\n");
    kfree(my_hrt_data_);

    /* queue resources */
    PR_INFO("release capture_event_buffer");
    release(&ess_work_struct_wrapper_.capture_event_buffer);
    PR_INFO("release event_bulk_data_buffer");
    release(&ess_work_struct_wrapper_.event_bulk_data_buffer);
}


/* support for select(), poll() and epoll() system calls
    POLLIN
        This bit must be set if the device can be read without blocking.
    POLLRDNORM
        This bit must be set if "normal" data is available for reading. A readable device returns (POLLIN | POLLRDNORM).
    POLLRDBAND
        This bit indicates that out-of-band data is available for reading from the device. It is currently used only in one place in the Linux kernel (the DECnet code) and is not generally applicable to device drivers.
    POLLPRI
        High-priority data (out-of-band) can be read without blocking. This bit causes select to report that an exception condition occurred on the file, because select reports out-of-band data as an exception condition.
    POLLHUP
        When a process reading this device sees end-of-file, the driver must set POLLHUP (hang-up). A process calling select is told that the device is readable, as dictated by the select functionality.
    POLLERR
        An error condition has occurred on the device. When poll is invoked, the device is reported as both readable and writable, since both read and write return an error code without blocking.
    POLLOUT
        This bit is set in the return value if the device can be written to without blocking.
    POLLWRNORM
        This bit has the same meaning as POLLOUT, and sometimes it actually is the same number. A writable device returns (POLLOUT | POLLWRNORM).
    POLLWRBAND
        Like POLLRDBAND, this bit means that data with nonzero priority can be written to the device. Only the datagram implementation of poll uses this bit, since a datagram can transmit out-of-band data.
*/
__poll_t gpio_irq_demo_poll(struct file *f, struct poll_table_struct *wait)
{
    unsigned int ret_val_mask = 0;

    PR_INFO("entry");
    PR_INFO("ess_work_struct_wrapper_.poll_enabled == %s", ((ess_work_struct_wrapper_.poll_enabled == true) ? "true" : "false"));

    poll_wait(f, &(ess_work_struct_wrapper_.capture_event_waitqueue), wait);

    PR_INFO("poll block release");

    /* validate data ready */
    mutex_lock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
    if (!empty(&ess_work_struct_wrapper_.event_bulk_data_buffer)) {
        PR_INFO("POLLIN | POLLRDNORM");
        ret_val_mask = POLLIN | POLLRDNORM;
    } else {
        PR_INFO("poll sleep");
        if (false == ess_work_struct_wrapper_.poll_enabled)
            ret_val_mask = POLLHUP;
    }
    mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);

    return ret_val_mask;
}

/* maintain a low overhead list of events between interrupt and work queue */
static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs)
{
    struct CircularBufferMod2* capture_event_buffer = &ess_work_struct_wrapper_.capture_event_buffer;

    static uint64_t counter = 0;

    PR_INFO("entry");
    PR_INFO("num_irqs_ = %d", ++num_irqs_);

    spin_lock(&ess_work_struct_wrapper_.capture_event_spinlock);
    PR_INFO("capture_event_spinlock locked");

    /* access capture_event_buffer */
    // log rising edge and count
    {
        struct CaptureEvent capture_event;
        capture_event.rising = 1;
        capture_event.event = counter++;
        PR_INFO("pushing event %lld", capture_event.event);
        push(capture_event_buffer, &capture_event);
    }

    spin_unlock(&ess_work_struct_wrapper_.capture_event_spinlock);
    PR_INFO("capture_event_spinlock unlocked");

    // TODO pass info in regs
    schedule_work(&ess_work_struct_wrapper_.event_work_struct);

    return (irq_handler_t) IRQ_HANDLED;
}


/* work queue pulls from event list and operates under mutex control with ess queue object */
static void ess_do_work(struct work_struct* work)
{
    struct ESSWorkStructWrapper* ess_work_struct_wrapper;
    struct CaptureEvent* capture_event_ref;
    struct CaptureEvent capture_event;
    unsigned long flags;
    uint64_t idx;

    PR_INFO("entry");

    spin_lock_irqsave(&ess_work_struct_wrapper_.capture_event_spinlock, flags);
    /* access capture_event_buffer */
    if (space(&ess_work_struct_wrapper_.capture_event_buffer)) {

        if (NULL != (capture_event_ref = front(&ess_work_struct_wrapper_.capture_event_buffer))) {
            /* copy event */
            PR_INFO("element_size : %zd", ess_work_struct_wrapper_.capture_event_buffer.element_size);
            memcpy(&capture_event, capture_event_ref, ess_work_struct_wrapper_.capture_event_buffer.element_size);
            pop(&ess_work_struct_wrapper_.capture_event_buffer);
            PR_INFO("capture_event : %s, %lld", (capture_event.rising == true) ? "rising" : "falling", capture_event.event);
        }
    }
    PR_INFO("capture_event_spinlock locked");
    spin_unlock_irqrestore(&ess_work_struct_wrapper_.capture_event_spinlock, flags);
    PR_INFO("capture_event_spinlock unlocked");

    ess_work_struct_wrapper = container_of(work, struct ESSWorkStructWrapper, event_work_struct);

    /* access event_bulk_data_buffer, shared by poll and read() */
    // TODO investigate interruptible version
    mutex_lock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
    PR_INFO("event_bulk_data_mtx locked");
    if (space(&ess_work_struct_wrapper_.event_bulk_data_buffer)) {
        if (NULL != capture_event_ref) {
            /* capture event obtained */
            // TODO consider providing event_bulk_data memory directly from buffer vs filling stack first then pushing
            struct EventBulkData event_bulk_data;
            memcpy(&event_bulk_data.capture_event, &capture_event, sizeof(struct CaptureEvent));
            for (idx = 0; idx < 10; idx++) {
                event_bulk_data.bulk_data[idx] = capture_event.event + idx;
            }

            if (0 == push(&ess_work_struct_wrapper_.event_bulk_data_buffer, &event_bulk_data)) {
                /* signal the blocked poll (select(), poll() or epoll() sys call) */
                PR_INFO("wake_up_interruptible()");
                wake_up_interruptible(&(ess_work_struct_wrapper->capture_event_waitqueue));
            }
        }
    }
    mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
    PR_INFO("event_bulk_data_mtx unlocked");
}