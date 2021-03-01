#pragma once

#include <linux/poll.h>

int gpio_oled_irq_init(void);
void gpio_oled_irq_exit(void);

ssize_t gpio_oled_irq_read(struct file *f, char __user *buff, size_t count, loff_t *pos);
__poll_t gpio_oled_irq_poll(struct file *f, struct poll_table_struct *wait);
long gpio_irq_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

