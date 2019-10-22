#pragma once

#include <linux/ioctl.h>

#define ESS_SET_GPIO_SEQ_NUM            0x01
#define ESS_CLR_GPIO_SEQ_NUM            0x02
#define ESS_DUTY_CYCLE_GPIO_SEQ_NUM     0x03

#define ESS_MAGIC 'E'
#define ESS_SET_GPIO                _IO(ESS_MAGIC, ESS_SET_GPIO_SEQ_NUM)
#define ESS_CLR_GPIO                _IO(ESS_MAGIC, ESS_CLR_GPIO_SEQ_NUM)
#define ESS_DUTY_CYCLE_GPIO         _IOW(ESS_MAGIC, ESS_DUTY_CYCLE_GPIO_SEQ_NUM, unsigned long)

#ifdef __cplusplus
extern "C" {
#endif

/* capture event metadata */
struct CaptureEvent {
    bool rising;            /* is rising edge */
    uint64_t event;         /* incrementing event number */
};

/* capture event bulk data */
struct EventBulkData {
    struct CaptureEvent capture_event;      /* raw capture event data */
    uint64_t bulk_data[10];                  /* bulk data associated with capture event */
};

#ifdef __cplusplus
}
#endif