#pragma once

#include <linux/fs.h>

int memory_init(char* proc_name);
void memory_exit(char* proc_name);
int memory_open(void);
void memory_close(void);
int memory_mmap(struct file *filep, struct vm_area_struct *vma);
