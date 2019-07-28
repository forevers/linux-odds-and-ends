#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "gpio_irq.h"

/* Rising edge triggered GPIO interrupt issuing kernal log message during IRQ.
   1.1 kohm R between the GPIO 27 input IRQ and driving GPIO.
   Use /sys/class/gpio user space gpio configution to toggle pin
*/

static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs);

static unsigned int gpio_num_irq_input_ = 27;
static unsigned int irq_number_;
static unsigned int num_irqs_ = 0;


int gpio_irq_demo_init(void)
{
    int result;

    /* gpio irq configuration */
    /* /sys/kernel/debug/gpio/<label> */
    if (0 == (result = gpio_request(gpio_num_irq_input_, "gpio_irq_input_label"))) {
        if (0 == (result = gpio_direction_input(gpio_num_irq_input_))) {
            /* does rpi not impl gpio_set_debounce() ? */
            //if (0 == (result = gpio_set_debounce(gpio_num_irq_input_, 200))) { // msec
            gpio_export(gpio_num_irq_input_, false);
                if (0 <= (irq_number_ = gpio_to_irq(gpio_num_irq_input_))) {
                    if (0 == (result = request_irq(irq_number_, (irq_handler_t)gpio_irq_handler, IRQF_TRIGGER_RISING, "gpio_irq_handler", NULL))) {
                        pr_err("irq configuration sucess\n");
                    } else {
                        pr_err("request_irq() failure: %d\n", result);
                    }
                } else {
                    pr_err("gpio_to_irq() failure: %d\n", irq_number_);
                }
            // } else {
            //     pr_err("gpio_set_debounce() failure: %d\n", result);
            // }
        } else {
            pr_err("gpio_direction_input() failure: %d\n", result);
        }
    } else {
        pr_err("gpio_request() failure: %d\n", result);
    }

    return result;
}


void gpio_irq_exit(void)
{
    pr_info("num_irqs_: %d\n", num_irqs_);
    free_irq(irq_number_, NULL);
    gpio_unexport(gpio_num_irq_input_);
    gpio_free(gpio_num_irq_input_);
}


static irq_handler_t gpio_irq_handler(unsigned int irq, void* dev_id, struct pt_regs* regs)
{
    pr_info("gpio_irq_handler()\n");
    pr_info("num_irqs_ = %d\n", ++num_irqs_);

    return (irq_handler_t) IRQ_HANDLED;
}
