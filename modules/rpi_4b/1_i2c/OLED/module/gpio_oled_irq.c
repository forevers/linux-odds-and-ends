// #include <linux/delay.h>
#include <linux/gpio.h>
// #include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
// #include <linux/workqueue.h>

#include "circ_buffer_pow2_elements.h"
// #include "gpio_irq.h"
// #include "gpio_irq_global.h"
#include "util.h"

/* This module uses the legacy interface for accessing and controlling the gpios.

    Rising edge triggered GPIO interrupt issuing kernel log message during IRQ.

    // user space query interrupt status
    $ cat /proc/interrupts
*/

static irq_handler_t gpio_btn_5_irq_handler(unsigned int irq, void* dev_id);

static irq_handler_t gpio_btn_6_irq_handler(unsigned int irq, void* dev_id);

static irq_handler_t gpio_rocker_4_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_rocker_17_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_rocker_23_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_rocker_22_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_rocker_27_irq_handler(unsigned int irq, void* dev_id);

static void do_work(struct work_struct* work);

// TODO dts assignment of gpio pins
#define BUTTON_5 0
#define BUTTON_6 1
#define ROCKER_D 2
#define ROCKER_N 3
#define ROCKER_S 4
#define ROCKER_E 6
#define ROCKER_W 7

static char button_5_[] = "oled button 5";
static char button_6_[] = "oled button 6";
static char rocker_4_[] = "oled rocker 4";
static char rocker_17_[] = "oled rocker 17";
static char rocker_22_[] = "oled rocker 22";
static char rocker_23_[] = "oled rocker 23";
static char rocker_27_[] = "oled rocker 27";

static unsigned int button_5_gpio_input_ = 5;
static unsigned int button_6_gpio_input_ = 6;
static unsigned int rocker_4_gpio_input_ = 4;
static unsigned int rocker_17_gpio_input_ = 17;
static unsigned int rocker_22_gpio_input_ = 22;
static unsigned int rocker_23_gpio_input_ = 23;
static unsigned int rocker_27_gpio_input_ = 27;

static unsigned int button_5_irq_number_;
static unsigned int button_6_irq_number_;
static unsigned int rocker_4_irq_number_;
static unsigned int rocker_17_irq_number_;
static unsigned int rocker_22_irq_number_;
static unsigned int rocker_23_irq_number_;
static unsigned int rocker_27_irq_number_;

static unsigned int num_irqs_ = 0;
static int gpio_can_sleep_;
static bool gpio_can_debounce_;

// /* high resolution timer used to drive gpo pwm */
// static struct HrtData {
//     struct hrtimer timer;
//     ktime_t period;
//     long period_sec;
//     unsigned long period_nsec;
// } *my_hrt_data_;

/* capture event metadata */
struct CaptureEvent {
    int button_num;         /* button number */
    uint64_t event;         /* incrementing event number */
};

/* capture event bulk data */
struct EventBulkData {
    struct CaptureEvent capture_event;      /* raw capture event data */
    uint64_t bulk_data[10];                  /* bulk data associated with capture event */
};

/* work queue for bottom half interrupt processing */
// TODO create custom ess workqueue */
struct ESSWorkStructWrapper {
    struct work_struct event_work_struct;
    struct CircularBufferPow2 capture_event_buffer;     /* upper half irq data */
    // TOD remove remove ... always enabled in this driver
    // bool poll_enabled;                                  /* device supports polling */ 
    wait_queue_head_t capture_event_waitqueue;          /* irq lower half processing by workqueue (or tasklet TODO) */
    uint64_t event;                                     /* incrementing event number */
    struct CircularBufferPow2 event_bulk_data_buffer;   /* lower half data */
    struct mutex event_bulk_data_mtx;                   /* event_bulk_data_buffer access mutex */

    unsigned int button_irq_numbers[7];
    irq_handler_t handlers[7];
} ess_work_struct_wrapper_;

// static enum hrtimer_restart my_kt_function(struct hrtimer *hr_timer)
// {
//     ktime_t now , interval;
//     int gpio_toggle_value = (gpio_get_value(gpio_num_output_) == 0) ? 1 : 0;

//     if (1 == gpio_can_sleep_) {
//         gpio_set_value_cansleep(gpio_num_output_, gpio_toggle_value);
//     } else {
//         gpio_set_value(gpio_num_output_, gpio_toggle_value);
//     }

//     PR_INFO("time_per_sec : %ld, time_per_nsec : %lu", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

//     now  = ktime_get();
//     interval = ktime_set(my_hrt_data_->period_sec, my_hrt_data_->period_nsec);
//     hrtimer_forward(hr_timer, now , interval);

//     // HRTIMER_RESTART timer is retarting vs HRTIMER_NORESTART timer is terminating
//     return HRTIMER_RESTART;
// }


/* configure rising edge interrupt handler irq_handler on interrupt number irq_number*/
int gpio_oled_irq(int button_idx, char* dev_name, unsigned int gpio_input, irq_handler_t irq_handler, unsigned int* irq_number)
{
    int result;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    // TODO consider to moving much of this to the open() call to allow potential HW sharing
    if (0 == (result = gpio_request(gpio_input, "gpio_input"))) {
        PR_INFO("gpio_request() success");

        if (0 == (result = gpio_direction_input(gpio_input))) {
            PR_INFO("gpio_direction_input() success");

            /* TODO - does rpi not impl gpio_set_debounce() */
            if (0 == (result = gpio_set_debounce(gpio_input, 200))) { // msec
                gpio_can_debounce_ = true;
            } else {
                PR_ERR("gpio_set_debounce() failure: %d", result);
                gpio_can_debounce_ = false;
            }

            /* allow presentation of gpio in sysfs, but do not allow userspace apps to control pin direction 
                  i.e. $ sudo cat /sys/kernel/debug/gpio
            */
            // gpio_export(gpio_input, false);
            if (0 <= (*irq_number = gpio_to_irq(gpio_input))) {
                // TODO pass as reference
                ess_work_struct_wrapper_.button_irq_numbers[button_idx] = *irq_number;
                ess_work_struct_wrapper_.handlers[button_idx] = irq_handler;

                if (0 != (result = request_irq(*irq_number, irq_handler, IRQF_TRIGGER_RISING, dev_name, NULL))) {
                    PR_ERR("request_irq() failure: %d", result);
                }
            } else {
                PR_ERR("gpio_to_irq() failure: %d", *irq_number);
            }
        } else {
            PR_ERR("gpio_direction_input() failure: %d", result);
            gpio_free(gpio_input);
        }
    } else {
        PR_ERR("gpio_request(%d) failure: %d", gpio_input, result);
    }

    return result;
}


int gpio_oled_irq_init(void)
{
    int result = -1;
    PR_INFO("entry");

    /* gpio irq upper half buffer and lower half work queue */
    if (0 == (result = init(&ess_work_struct_wrapper_.capture_event_buffer, sizeof(struct CaptureEvent), 0x08))) {
        PR_INFO("init() success");

        if (0 == (result = init(&ess_work_struct_wrapper_.event_bulk_data_buffer, sizeof(struct EventBulkData), 0x10))) {
            PR_INFO("init() success");

            ess_work_struct_wrapper_.event = 0;
            // ess_work_struct_wrapper_.poll_enabled = false;

            /* irq reads fill capture_event_buffer */
            init_waitqueue_head(&ess_work_struct_wrapper_.capture_event_waitqueue);

            INIT_WORK(&ess_work_struct_wrapper_.event_work_struct, do_work);

            // sema_init(&(ess_work_struct_wrapper_->sem), 1);
            mutex_init(&ess_work_struct_wrapper_.event_bulk_data_mtx);

        } else {
            PR_ERR("init() EventBulkData failure: %d", result);
            release(&ess_work_struct_wrapper_.capture_event_buffer);
        }                   
    } else {
        PR_ERR("init() CaptureEvent failure: %d", result);
    }
    if (result != 0) return result;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    // TODO consider to moving much of this to the open() call to allow potential HW sharing
    if (0 == (result = gpio_request(button_5_gpio_input_, "button_5_gpio_input_label"))) {
        PR_INFO("gpio_request() success");

        if (0 == (result = gpio_direction_input(button_5_gpio_input_))) {
            PR_INFO("gpio_direction_input() success");

            /* TODO - does rpi not impl gpio_set_debounce() */
            if (0 == (result = gpio_set_debounce(button_5_gpio_input_, 200))) { // msec
                gpio_can_debounce_ = true;
            } else {
                PR_ERR("gpio_set_debounce() failure: %d", result);
                gpio_can_debounce_ = false;
            }

            /* allow presentation of gpio in sysfs, but do not allow userspace apps to control pin direction 
                  i.e. $ sudo cat /sys/kernel/debug/gpio
            */
            // gpio_export(button_5_gpio_input_, false);
            if (0 <= (button_5_irq_number_ = gpio_to_irq(button_5_gpio_input_))) {
                ess_work_struct_wrapper_.button_irq_numbers[BUTTON_5] = button_5_irq_number_;

                // TODO pass device object to driver in last parameter
                if (0 != (result = request_irq(button_5_irq_number_, (irq_handler_t)gpio_btn_5_irq_handler, IRQF_TRIGGER_RISING, "gpio_btn_5_irq_handler", NULL))) {
                    PR_ERR("request_irq() failure: %d", result);
                }
            } else {
                PR_ERR("gpio_to_irq() failure: %d", button_5_irq_number_);
            }
        } else {
            PR_ERR("gpio_direction_input() failure: %d", result);
            gpio_free(button_5_gpio_input_);
        }
    } else {
        PR_ERR("gpio_request() failure: %d", result);
    }
    if (result != 0) return result;

    if (0 != (result = gpio_oled_irq(BUTTON_6, button_6_, button_6_gpio_input_, (irq_handler_t)gpio_btn_6_irq_handler, &button_6_irq_number_))) {
        PR_ERR("button_6_irq_number_ configuration failure: %d", result);
        return result;
    }
    if (0 != (result = gpio_oled_irq(ROCKER_D, rocker_4_, rocker_4_gpio_input_, (irq_handler_t)gpio_rocker_4_irq_handler, &rocker_4_irq_number_))) {
        PR_ERR("rocker_4_irq_number_ configuration failure: %d", result);
        return result;
    }
    if (0 != (result = gpio_oled_irq(ROCKER_N, rocker_17_, rocker_17_gpio_input_, (irq_handler_t)gpio_rocker_17_irq_handler, &rocker_17_irq_number_))) {
        PR_ERR("rocker_17_irq_number_ configuration failure: %d", result);
        return result;
    }
    if (0 != (result = gpio_oled_irq(ROCKER_S, rocker_22_, rocker_22_gpio_input_, (irq_handler_t)gpio_rocker_22_irq_handler, &rocker_22_irq_number_))) {
        PR_ERR("rocker_22_irq_number_ configuration failure: %d", result);
        return result;
    }
    if (0 != (result = gpio_oled_irq(ROCKER_E, rocker_23_, rocker_23_gpio_input_, (irq_handler_t)gpio_rocker_23_irq_handler, &rocker_23_irq_number_))) {
        PR_ERR("rocker_23_irq_number_ configuration failure: %d", result);
        return result;
    }
    if (0 != (result = gpio_oled_irq(ROCKER_W, rocker_27_, rocker_27_gpio_input_, (irq_handler_t)gpio_rocker_27_irq_handler, &rocker_27_irq_number_))) {
        PR_ERR("rocker_27_irq_number_ configuration failure: %d", result);
        return result;
    }

    PR_INFO("exit");
    return result;
}

/* pulls from a buffer */
ssize_t gpio_oled_irq_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    ssize_t bytes_read = 0;

    PR_INFO("entry");

    PR_INFO("count passed: %zu, expect mod of %lu", count, sizeof(struct EventBulkData));

    // if (1 == count) {

    //     PR_INFO("read current value");

    //     /* read current value */
    //     *buff = gpio_get_value(gpio_num_output_);
    //     return 1;

    // } else 
    if ((count % sizeof(struct EventBulkData)) == 0) {

        struct EventBulkData* event_bulk_data;
        struct CircularBufferPow2* event_bulk_data_buffer;

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


// ssize_t gpio_irq_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos)
// {
//     size_t counts_remaining = count;

//     PR_INFO("entry");
//     PR_INFO("count = %d", count);

//     /* must be value/delay pair */
//     if (count % 2) return (ssize_t)0;

//     /* cancel timer if active */
//     hrtimer_cancel(&my_hrt_data_->timer);

//     /* write value and msec sleep count */
//     PR_INFO("start gpio sequence");
//     while (counts_remaining) {
//         PR_INFO("val = %d", (*buff == 0) ? 0 : 1);
//         gpio_set_value_cansleep(gpio_num_output_, (*buff == 0) ? 0 : 1);
//         buff++;
//         mdelay(*buff);
//         buff++;
//         counts_remaining -= 2;
//     }

//     return (ssize_t)(count/2);
// }


// long gpio_irq_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
// {
//     long retval = 0;

//     PR_INFO("gpio_irq_demo_ioctl()");

//     switch(cmd) {

//         case ESS_SET_GPIO:

//             PR_INFO("ESS_SET_GPIO_SEQ_NUM");
//             // cancel timer if active
//             hrtimer_cancel(&my_hrt_data_->timer);

//             /* set gpo value */
//             gpio_set_value_cansleep(gpio_num_output_, 1);

//             // TODO have a statefull behavior for handling ESS_DUTY_CYCLE_GPIO mode vs set/get mode
//             /* signal any blocked poll (select(), poll() or epoll() sys call) */
//             ess_work_struct_wrapper_.poll_enabled = false;
//             wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));

//             break;

//         case ESS_CLR_GPIO:

//             PR_INFO("ESS_CLR_GPIO_SEQ_NUM");
//             // cancel timer if active
//             hrtimer_cancel(&my_hrt_data_->timer);

//             /* clear gpo */
//             gpio_set_value_cansleep(gpio_num_output_, 0);

//             // TODO have a statefull behavior for handling ESS_DUTY_CYCLE_GPIO mode vs set/get mode ... and sync the release of the user space call
//             /* signal any blocked poll (select(), poll() or epoll() sys call) */
//             ess_work_struct_wrapper_.poll_enabled = false;
//             wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));
//             break;

//         case ESS_DUTY_CYCLE_GPIO:

//             PR_INFO("ESS_DUTY_CYCLE_GPIO_SEQ_NUM");
//             PR_INFO("period : %lu msec", arg);

//             if (arg == 0) {

//                 PR_INFO("cancel duty cycle mode");
//                 if (hrtimer_cancel(&my_hrt_data_->timer)) {
//                     pr_info("cancelled active timer\n");
//                 }

//                 ess_work_struct_wrapper_.poll_enabled = false;
//                 wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));

//             } else {

//                 my_hrt_data_->period_sec = arg / 1000;
//                 my_hrt_data_->period_nsec = (arg - my_hrt_data_->period_sec*1000) *1000000;
//                 PR_INFO("time_per_sec : %ld, time_per_nsec : %lu", my_hrt_data_->period_sec, my_hrt_data_->period_nsec);

//                 ess_work_struct_wrapper_.poll_enabled = true;

//                 my_hrt_data_->period = ktime_set(my_hrt_data_->period_sec, my_hrt_data_->period_nsec);
//                 // cancel and start new timer at specified period
//                 hrtimer_cancel(&my_hrt_data_->timer);
//                 hrtimer_start(&my_hrt_data_->timer, my_hrt_data_->period, HRTIMER_MODE_REL);

//             }
//             break;

//         default:

//             retval = -EPERM;
//             break;

//         return retval;
//     }

//     return retval;
// }


void gpio_oled_irq_exit(void)
{
    // TODO consider to moving much of this to the close() call to allow potential HW sharing

    PR_INFO("entry");
    PR_INFO("num_irqs_: %d", num_irqs_);

    PR_INFO("gpio free %d", button_5_irq_number_);
    gpio_free(button_5_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[BUTTON_5]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[BUTTON_5], NULL);

    PR_INFO("gpio free %d", button_6_irq_number_);
    gpio_free(button_6_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[BUTTON_6]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[BUTTON_6], NULL);

    PR_INFO("gpio free %d", rocker_4_irq_number_);
    gpio_free(rocker_4_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[ROCKER_D]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[ROCKER_D], NULL);

    PR_INFO("gpio free %d", rocker_17_irq_number_);
    gpio_free(rocker_17_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[ROCKER_N]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[ROCKER_N], NULL);

    PR_INFO("gpio free %d", rocker_22_irq_number_);
    gpio_free(rocker_22_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[ROCKER_S]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[ROCKER_S], NULL);

    PR_INFO("gpio free %d", rocker_23_irq_number_);
    gpio_free(rocker_23_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[ROCKER_E]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[ROCKER_E], NULL);

    PR_INFO("gpio free %d", rocker_27_irq_number_);
    gpio_free(rocker_27_gpio_input_);
    PR_INFO("free irq %d", ess_work_struct_wrapper_.button_irq_numbers[ROCKER_W]);
    free_irq(ess_work_struct_wrapper_.button_irq_numbers[ROCKER_W], NULL);

    // gpio_unexport(gpio_num_output_);

    // /* timer resources */
    // PR_INFO("cancel active time");
    // if (hrtimer_cancel(&my_hrt_data_->timer)) {
    //     PR_INFO("cancelled active timer");
    // }
    // PR_INFO("free timer object\n");
    // kfree(my_hrt_data_);

    /* release workqueue */
    // flush_workqueue(&ess_work_struct_wrapper_.event_work_struct);
    // destroy_workqueue(&ess_work_struct_wrapper_.event_work_struct);
    flush_scheduled_work();

    /* queue resources */
    PR_INFO("release capture_event_buffer");
    release(&ess_work_struct_wrapper_.capture_event_buffer);
    PR_INFO("release event_bulk_data_buffer");
    release(&ess_work_struct_wrapper_.event_bulk_data_buffer);

    PR_INFO("exit");
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
__poll_t gpio_oled_irq_poll(struct file *f, struct poll_table_struct *wait)
{
    unsigned int ret_val_mask = 0;

    PR_INFO("entry");

    poll_wait(f, &(ess_work_struct_wrapper_.capture_event_waitqueue), wait);

    PR_INFO("poll block release");

    /* validate data ready */
    mutex_lock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
    if (!empty(&ess_work_struct_wrapper_.event_bulk_data_buffer)) {
        PR_INFO("POLLIN | POLLRDNORM");
        ret_val_mask = POLLIN | POLLRDNORM;
    } else {
        PR_INFO("poll sleep");
    }
    mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);

    return ret_val_mask;
}

/* maintain a low overhead list of events between interrupt and work queue */
static irq_handler_t gpio_btn_5_irq_handler(unsigned int irq, void* dev_id)
{
    struct CircularBufferPow2* capture_event_buffer = &ess_work_struct_wrapper_.capture_event_buffer;

    static uint64_t counter = 0;
    struct CaptureEvent capture_event;

    PR_INFO("num_irqs_ = %d", ++num_irqs_);
    PR_ERR("irq %u", irq);

    capture_event.button_num = BUTTON_5;

    capture_event.event = ++counter;
    PR_INFO("pushing event %lld", capture_event.event);
    push(capture_event_buffer, &capture_event);

    schedule_work(&ess_work_struct_wrapper_.event_work_struct);

    return (irq_handler_t) IRQ_HANDLED;
}


static irq_handler_t gpio_btn_6_irq_handler(unsigned int irq, void* dev_id)
{
    struct CircularBufferPow2* capture_event_buffer = &ess_work_struct_wrapper_.capture_event_buffer;

    static uint64_t counter = 0;
    struct CaptureEvent capture_event;

    PR_ERR("ess_work_struct_wrapper_ @ 0x%p", dev_id);
    PR_INFO("entry");
    PR_INFO("num_irqs_ = %d", ++num_irqs_);
    PR_ERR("irq %u", irq);

    /* access capture_event_buffer */
    capture_event.event = ++counter;
    PR_INFO("pushing event %lld", capture_event.event);
    push(capture_event_buffer, &capture_event);

    schedule_work(&ess_work_struct_wrapper_.event_work_struct);

    return (irq_handler_t) IRQ_HANDLED;
}


static irq_handler_t gpio_rocker_4_irq_handler(unsigned int irq, void* dev_id)
{
    PR_INFO("depress rocker");
    return (irq_handler_t) IRQ_HANDLED;
}
static irq_handler_t gpio_rocker_17_irq_handler(unsigned int irq, void* dev_id)
{
    PR_INFO("north rocker");
    return (irq_handler_t) IRQ_HANDLED;
}
static irq_handler_t gpio_rocker_22_irq_handler(unsigned int irq, void* dev_id)
{
    PR_INFO("south rocker");
    return (irq_handler_t) IRQ_HANDLED;
}
static irq_handler_t gpio_rocker_23_irq_handler(unsigned int irq, void* dev_id)
{
    PR_INFO("east rocker");
    return (irq_handler_t) IRQ_HANDLED;
}
static irq_handler_t gpio_rocker_27_irq_handler(unsigned int irq, void* dev_id)
{
    PR_INFO("west rocker");
    return (irq_handler_t) IRQ_HANDLED;
}


/* work queue pulls from event list and operates under mutex control with ess queue object */
/* TODO investigate threaded processing to process work events */
static void do_work(struct work_struct* work)
{
    struct ESSWorkStructWrapper* ess_work_struct_wrapper;
    struct CaptureEvent* capture_event_ref;
    struct CaptureEvent capture_event;
    // unsigned long flags;
    uint64_t idx;
    static int test_entry = 0;

    PR_ERR("entry: %d", ++test_entry);

    ess_work_struct_wrapper = container_of(work, struct ESSWorkStructWrapper, event_work_struct);

    // TODO producer - consumer barrier model
    /* access capture_event_buffer */
    while (NULL != (capture_event_ref = front(&ess_work_struct_wrapper->capture_event_buffer))) {
        /* copy event */
        PR_INFO("element_size : %zd", ess_work_struct_wrapper->capture_event_buffer.element_size);
        memcpy(&capture_event, capture_event_ref, ess_work_struct_wrapper->capture_event_buffer.element_size);
        pop(&ess_work_struct_wrapper->capture_event_buffer);
        PR_INFO("button_num : %d, %lld", capture_event.button_num, capture_event.event);

        /* access event_bulk_data_buffer, shared by poll and read() */
        // TODO investigate interruptable version
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
                    PR_INFO("wake_up_interruptable()");
                    wake_up_interruptible(&(ess_work_struct_wrapper->capture_event_waitqueue));
                }
            }
        }
        mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
        PR_INFO("event_bulk_data_mtx unlocked");
    }
}