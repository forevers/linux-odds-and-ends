#pragma once

#include <linux/poll.h>

int gpio_irq_demo_init(void);
void gpio_irq_exit(void);

ssize_t gpio_irq_demo_read(struct file *f, char __user *buff, size_t count, loff_t *pos);
ssize_t gpio_irq_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos);
long gpio_irq_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

__poll_t gpio_irq_demo_poll(struct file *f, struct poll_table_struct *wait);

