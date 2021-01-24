#pragma once

#include <linux/poll.h>

int gpio_oled_irq_init(void);
void gpio_oled_irq_exit(void);

// ssize_t gpio_oled_demo_read(struct file *f, char __user *buff, size_t count, loff_t *pos);
// ssize_t gpio_oled_demo_write(struct file *f, const char __user *buff, size_t count, loff_t *pos);
// long gpio_oled_demo_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

__poll_t gpio_irq_oled_poll(struct file *f, struct poll_table_struct *wait);

