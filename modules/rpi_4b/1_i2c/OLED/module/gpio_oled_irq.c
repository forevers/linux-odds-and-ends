#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include "circ_buffer_pow2_elements.h"
#include "i2c_oled_global.h"
#include "util.h"

/*
    // user space query interrupt status
    $ cat /proc/interrupts

    // sudo python ~/projects/oled/examples/ssd1306_bonnet_buttons.py prime the rocker gpio configuration
    
    References:
        https://www.kernel.org/doc/Documentation/gpio/consumer.txt
*/

/* 2 buttons and 1 rocker switch */
static irq_handler_t gpio_btn_5_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_btn_6_irq_handler(unsigned int irq, void* dev_id);
static irq_handler_t gpio_rocker_irq_handler(unsigned int irq, void* dev_id);

/* lower half processing */
static void do_work(struct work_struct* work);

static char btn_5_desc[] = "oled button 5";
static char btn_6_desc[] = "oled button 6";
static char rocker_down_desc[] = "oled rocker down";
static char rocker_north_desc[] = "oled rocker north";
static char rocker_south_desc[] = "oled rocker south";
static char rocker_east_desc[] = "oled rocker east";
static char rocker_west_desc[] = "oled rocker west";

static unsigned int num_irqs_ = 0;
static bool gpio_can_debounce_;

struct GpioConfig {
    char* description;
    irq_handler_t handler;
    struct gpio_desc *desc;
    unsigned int gpio_number;
    unsigned int gpio_irq_number;
};

static struct GpioConfig gpio_config_[] = {
    {btn_5_desc, (irq_handler_t)gpio_btn_5_irq_handler},
    {btn_6_desc, (irq_handler_t)gpio_btn_6_irq_handler},
    {rocker_down_desc, (irq_handler_t)gpio_rocker_irq_handler},
    {rocker_north_desc, (irq_handler_t)gpio_rocker_irq_handler},
    {rocker_south_desc, (irq_handler_t)gpio_rocker_irq_handler},
    {rocker_east_desc, (irq_handler_t)gpio_rocker_irq_handler},
    {rocker_west_desc, (irq_handler_t)gpio_rocker_irq_handler}
};

/* work queue for bottom half interrupt processing */
// TODO create custom ess workqueue */
struct ESSWorkStructWrapper {
    struct work_struct event_work_struct;
    struct CircularBufferPow2 capture_event_buffer;     /* upper half irq data */
    bool poll_enabled;                                  /* set to false to terminate user space blocking poll */ 
    wait_queue_head_t capture_event_waitqueue;          /* irq lower half processing by workqueue (or tasklet TODO) */
    uint64_t event;                                     /* incrementing event number */
    struct CircularBufferPow2 event_bulk_data_buffer;   /* lower half data */
    struct mutex event_bulk_data_mtx;                   /* event_bulk_data_buffer access mutex */
} ess_work_struct_wrapper_;


/* configure gpio interrupt */
int gpio_oled_irq(struct device *dev, int button_idx, irq_handler_t irq_handler, char* dts_name, int dts_index)
{
    int result;
    int retval;
    int irq_number;
    struct gpio_desc *desc;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    // TODO consider to moving much of this to the open() call to allow potential HW sharing

    if (dts_index) {
        desc = gpiod_get_index(dev, dts_name, dts_index, GPIOD_IN);
    } else {
        desc = gpiod_get(dev, dts_name, GPIOD_IN);
    }
    if (IS_ERR(gpio_config_[button_idx].desc)) {
        PR_ERR("failed to acquire GPIO\n");
        retval = -ENXIO;
    }
    gpio_config_[button_idx].desc = desc;

    /* TODO - does rpi not impl gpio_set_debounce() */
    if (0 == (result = gpiod_set_debounce(desc, 200))) { // msec
        gpio_can_debounce_ = true;
    } else {
        PR_ERR("gpio_set_debounce() failure: %d", result);
        gpio_can_debounce_ = false;
    }

    /* allow presentation of gpio in sysfs, but do not allow userspace apps to control pin direction 
            i.e. $ sudo cat /sys/kernel/debug/gpio
    */
    // gpio_export(gpio_input, false);
    if (0 <= (irq_number = gpiod_to_irq(desc))) {
        // TODO pass as reference
        // ess_work_struct_wrapper_.button_irq_numbers[button_idx] = irq_number;
        // ess_work_struct_wrapper_.handlers[button_idx] = irq_handler;

        gpio_config_[button_idx].gpio_irq_number = irq_number;
        gpio_config_[button_idx].gpio_number = desc_to_gpio(desc);
        PR_INFO("gpio: %d, irq: %d", gpio_config_[button_idx].gpio_number, irq_number);

        if (0 != (result = request_irq(irq_number, irq_handler, IRQF_TRIGGER_RISING, gpio_config_[button_idx].description, NULL))) {
            PR_ERR("request_irq() failure: %d", result);
        }
    } else {
        PR_ERR("gpio_to_irq() failure: %d", irq_number);
    }

    return result;
}


/* claim gpio resources */
int gpio_oled_probe(struct device *dev)
{
    int retval = 0;
    int result;

    PR_INFO("gpio_oled_probe() entry");

    if (0 == (result = gpio_oled_irq(dev, BUTTON_5, (irq_handler_t)gpio_btn_5_irq_handler, "btn5irq", 0))) {
        if (0 == (result = gpio_oled_irq(dev, BUTTON_6, (irq_handler_t)gpio_btn_6_irq_handler, "btn6irq", 0))) {
            if (0 == (result = gpio_oled_irq(dev, ROCKER_D, (irq_handler_t)gpio_rocker_irq_handler, "rockerirq", 0))) {
                if (0 == (result = gpio_oled_irq(dev, ROCKER_N, (irq_handler_t)gpio_rocker_irq_handler, "rockerirq", 1))) {
                    if (0 == (result = gpio_oled_irq(dev, ROCKER_S, (irq_handler_t)gpio_rocker_irq_handler, "rockerirq", 2))) {
                        if (0 == (result = gpio_oled_irq(dev, ROCKER_E, (irq_handler_t)gpio_rocker_irq_handler, "rockerirq", 3))) {
                            if (0 != (result = gpio_oled_irq(dev, ROCKER_W, (irq_handler_t)gpio_rocker_irq_handler, "rockerirq", 4))) {
                                PR_ERR("ROCKER_W configuration failure: %d", result);
                                gpiod_put(gpio_config_[ROCKER_W].desc);
                                gpiod_put(gpio_config_[ROCKER_E].desc);
                                gpiod_put(gpio_config_[ROCKER_S].desc);
                                gpiod_put(gpio_config_[ROCKER_N].desc);
                                gpiod_put(gpio_config_[ROCKER_D].desc);
                                gpiod_put(gpio_config_[BUTTON_5].desc);
                                retval = result;
                            }
                        } else {
                            PR_ERR("ROCKER_E configuration failure: %d", result);
                            gpiod_put(gpio_config_[ROCKER_E].desc);
                            gpiod_put(gpio_config_[ROCKER_S].desc);
                            gpiod_put(gpio_config_[ROCKER_N].desc);
                            gpiod_put(gpio_config_[ROCKER_D].desc);
                            gpiod_put(gpio_config_[BUTTON_5].desc);
                            retval = result;
                        }
                    } else {
                        PR_ERR("ROCKER_S configuration failure: %d", result);
                        gpiod_put(gpio_config_[ROCKER_S].desc);
                        gpiod_put(gpio_config_[ROCKER_N].desc);
                        gpiod_put(gpio_config_[ROCKER_D].desc);
                        gpiod_put(gpio_config_[BUTTON_5].desc);
                        retval = result;
                    }
                } else {
                    PR_ERR("ROCKER_N configuration failure: %d", result);
                    gpiod_put(gpio_config_[ROCKER_N].desc);
                    gpiod_put(gpio_config_[ROCKER_D].desc);
                    gpiod_put(gpio_config_[BUTTON_5].desc);
                    retval = result;
                }
            } else {
                PR_ERR("ROCKER_D configuration failure: %d", result);
                gpiod_put(gpio_config_[ROCKER_D].desc);
                gpiod_put(gpio_config_[BUTTON_5].desc);
                retval = result;
            }
        } else {
            PR_ERR("BUTTON_6 configuration failure: %d", result);
            gpiod_put(gpio_config_[BUTTON_5].desc);
            retval = result;
        }
    } else {
        PR_ERR("BUTTON_5 configuration failure: %d", result);
        retval = result;
    }

    PR_INFO("gpio_oled_probe() exit");
    return retval;
}


/* release gpio resources */
int gpio_oled_remove(struct device *dev)
{
    PR_INFO("gpio_oled_remove() entry");

    gpiod_put(gpio_config_[BUTTON_5].desc);
    gpiod_put(gpio_config_[BUTTON_6].desc);
    gpiod_put(gpio_config_[ROCKER_D].desc);
    gpiod_put(gpio_config_[ROCKER_N].desc);
    gpiod_put(gpio_config_[ROCKER_S].desc);
    gpiod_put(gpio_config_[ROCKER_E].desc);
    gpiod_put(gpio_config_[ROCKER_W].desc);

    PR_INFO("gpio_oled_remove() exit");
    return 0;
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
            ess_work_struct_wrapper_.poll_enabled = true;

            /* irq reads fill capture_event_buffer */
            init_waitqueue_head(&ess_work_struct_wrapper_.capture_event_waitqueue);

            INIT_WORK(&ess_work_struct_wrapper_.event_work_struct, do_work);

            mutex_init(&ess_work_struct_wrapper_.event_bulk_data_mtx);

        } else {
            PR_ERR("init() EventBulkData failure: %d", result);
            release(&ess_work_struct_wrapper_.capture_event_buffer);
        }                   
    } else {
        PR_ERR("init() CaptureEvent failure: %d", result);
    }
    if (result != 0) return result;

    PR_INFO("exit");
    return result;
}

/* pulls from a buffer */
ssize_t gpio_oled_irq_read(struct file *f, char __user *buff, size_t count, loff_t *pos)
{
    ssize_t bytes_read = 0;

    // PR_INFO("entry");
    // PR_INFO("count passed: %zu, expect mod of %lu", count, sizeof(struct EventBulkData));

    if ((count % sizeof(struct EventBulkData)) == 0) {

        struct EventBulkData* event_bulk_data;
        struct CircularBufferPow2* event_bulk_data_buffer;

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
        /* invalid read request size */
        PR_ERR("invalid read request size");
        bytes_read = -EPERM;
    }

    PR_INFO("bytes_read: %ld", bytes_read);
    return bytes_read;
}


long gpio_irq_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    long retval = 0;

    PR_INFO("entry()");

    switch (cmd) {
        case IOCTL_RELEASE_POLL:
            ess_work_struct_wrapper_.poll_enabled = false;
            wake_up_interruptible(&(ess_work_struct_wrapper_.capture_event_waitqueue));
            break;
    }

    return retval;
}


/* free gpio and its irq */
static void release_gpio_resource(unsigned int gpio)
{
    PR_INFO("gpio free %d", gpio_config_[gpio].gpio_number);
    gpio_free(gpio_config_[gpio].gpio_number);
    PR_INFO("free irq %d", gpio_config_[gpio].gpio_irq_number);
    free_irq(gpio_config_[gpio].gpio_irq_number, NULL);
}


void gpio_oled_irq_exit(void)
{
    // TODO consider to moving much of this to the close() call to allow potential HW sharing

    PR_INFO("entry");
    PR_INFO("num_irqs_: %d", num_irqs_);

    /* free gpio and its irq */
    release_gpio_resource(BUTTON_5);
    release_gpio_resource(BUTTON_6);
    release_gpio_resource(ROCKER_D);
    release_gpio_resource(ROCKER_N);
    release_gpio_resource(ROCKER_S);
    release_gpio_resource(ROCKER_E);
    release_gpio_resource(ROCKER_W);

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
        // PR_INFO("POLLIN | POLLRDNORM");
        ret_val_mask = POLLIN | POLLRDNORM;
    } else {
        if (false == ess_work_struct_wrapper_.poll_enabled) {
            PR_INFO("poll sleep");
            ret_val_mask = POLLHUP;
        } 
        // else {
        //     ret_val_mask = POLLIN | POLLRDNORM;
        // }
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

    static unsigned long last_jiffy = 0;
    unsigned long current_jiffy;
    long time_diff;
    long msec;

    if (!gpio_can_debounce_) {
        current_jiffy = jiffies;
        time_diff = (long)current_jiffy - (long)last_jiffy;
        msec = time_diff * 1000 / HZ;

        if (msec < 20) {
            PR_ERR("debounce msec: %ld", msec);
            return (irq_handler_t) IRQ_HANDLED;
        }
    }

    // PR_INFO("num_irqs_ = %d", ++num_irqs_);
    // PR_ERR("irq %u", irq);

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

    static unsigned long last_jiffy = 0;
    unsigned long current_jiffy;
    long time_diff;
    long msec;

    if (!gpio_can_debounce_) {
        current_jiffy = jiffies;
        time_diff = (long)current_jiffy - (long)last_jiffy;
        msec = time_diff * 1000 / HZ;

        if (msec < 20) {
            PR_ERR("debounce msec: %ld", msec);
            return (irq_handler_t) IRQ_HANDLED;
        }
    }

    // PR_INFO("num_irqs_ = %d", ++num_irqs_);
    // PR_ERR("irq %u", irq);

    /* access capture_event_buffer */
    capture_event.button_num = BUTTON_6;
    capture_event.event = ++counter;
    PR_INFO("pushing event %llu", capture_event.event);
    push(capture_event_buffer, &capture_event);

    schedule_work(&ess_work_struct_wrapper_.event_work_struct);

    return (irq_handler_t) IRQ_HANDLED;
}


static irq_handler_t gpio_rocker_irq_handler(unsigned int irq, void* dev_id)
{
    struct CircularBufferPow2* capture_event_buffer = &ess_work_struct_wrapper_.capture_event_buffer;
    static uint64_t counter = 0;
    struct CaptureEvent capture_event;

    static unsigned long last_jiffy = 0;
    unsigned long current_jiffy;
    long time_diff;
    long msec;

    if (!gpio_can_debounce_) {
        current_jiffy = jiffies;
        time_diff = (long)current_jiffy - (long)last_jiffy;
        msec = time_diff * 1000 / HZ;

        if (msec < 20) {
            PR_ERR("debounce msec: %ld", msec);
            return (irq_handler_t) IRQ_HANDLED;
        }
    }

    PR_INFO("msec: %ld", msec);
    last_jiffy = current_jiffy;

    // PR_INFO("num_irqs_ = %d", ++num_irqs_);
    // PR_ERR("irq %u", irq);

    if (irq == gpio_config_[ROCKER_D].gpio_irq_number) {
        capture_event.button_num = ROCKER_D;
        PR_INFO("1%s", gpio_config_[ROCKER_D].description);
    } else if (irq == gpio_config_[ROCKER_N].gpio_irq_number) {
        capture_event.button_num = ROCKER_N;
        PR_INFO("2%s", gpio_config_[ROCKER_N].description);
    } else if (irq == gpio_config_[ROCKER_S].gpio_irq_number) {
        capture_event.button_num = ROCKER_S;
        PR_INFO("3%s", gpio_config_[ROCKER_S].description);
    } else if (irq == gpio_config_[ROCKER_E].gpio_irq_number) {
        capture_event.button_num = ROCKER_E;
        PR_INFO("4%s", gpio_config_[ROCKER_E].description);
    } else if (irq == gpio_config_[ROCKER_W].gpio_irq_number) {
        capture_event.button_num = ROCKER_W;
        PR_INFO("5%s", gpio_config_[ROCKER_W].description);
    }

    /* access capture_event_buffer */
    capture_event.event = ++counter;
    // PR_INFO("pushing event %llu", capture_event.event);
    push(capture_event_buffer, &capture_event);

    schedule_work(&ess_work_struct_wrapper_.event_work_struct);

    return (irq_handler_t) IRQ_HANDLED;
}


/* work queue pulls from event list and operates under mutex control with ess queue object */
// TODO investigate threaded processing to process work events
static void do_work(struct work_struct* work)
{
    struct ESSWorkStructWrapper* ess_work_struct_wrapper;
    struct CaptureEvent* capture_event_ref;
    struct CaptureEvent capture_event;
    uint64_t idx;

    ess_work_struct_wrapper = container_of(work, struct ESSWorkStructWrapper, event_work_struct);

    // TODO producer - consumer barrier model
    /* access capture_event_buffer */
    while (NULL != (capture_event_ref = front(&ess_work_struct_wrapper->capture_event_buffer))) {
        /* copy event */
        // PR_INFO("element_size : %zd", ess_work_struct_wrapper->capture_event_buffer.element_size);
        memcpy(&capture_event, capture_event_ref, ess_work_struct_wrapper->capture_event_buffer.element_size);
        pop(&ess_work_struct_wrapper->capture_event_buffer);
        PR_INFO("button_num : %d, num irqs: %llu", capture_event.button_num, capture_event.event);

        /* access event_bulk_data_buffer, shared by poll and read() */
        // TODO investigate interruptable version
        mutex_lock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
        // PR_INFO("event_bulk_data_mtx locked");
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
                    PR_ERR("wake_up_interruptable()");
                    wake_up_interruptible(&(ess_work_struct_wrapper->capture_event_waitqueue));
                }
            }
        }
        mutex_unlock(&ess_work_struct_wrapper_.event_bulk_data_mtx);
        // PR_INFO("event_bulk_data_mtx unlocked");
    }
}
