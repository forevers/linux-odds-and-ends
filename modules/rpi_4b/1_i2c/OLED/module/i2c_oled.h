#pragma once

int ess_oled_init(void);
void ess_oled_cleanup(void);

ssize_t ess_oled_read(struct file *f, char __user *buff, size_t count, loff_t *pos);
ssize_t ess_oled_write(struct file *f, const char __user *buff, size_t count, loff_t *pos);
long ess_oled_ioctl(struct file *f, unsigned int cmd, unsigned long arg);
__poll_t ess_oled_poll(struct file *f, struct poll_table_struct *wait);

