#pragma once

#include <linux/ioctl.h>

#define ESS_SET_GPIO_SEQ_NUM            0x01
#define ESS_CLR_GPIO_SEQ_NUM            0x02
#define ESS_DUTY_CYCLE_GPIO_SEQ_NUM     0x03

#define ESS_MAGIC 'E'
#define ESS_SET_GPIO            _IO(ESS_MAGIC, ESS_SET_GPIO_SEQ_NUM)
#define ESS_CLR_GPIO            _IO(ESS_MAGIC, ESS_CLR_GPIO_SEQ_NUM)
#define ESS_DUTY_CYCLE_GPIO     _IOW(ESS_MAGIC, ESS_DUTY_CYCLE_GPIO_SEQ_NUM, unsigned long)