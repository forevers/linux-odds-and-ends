#pragma once

#include <linux/ioctl.h>

/* ioctl and write commands */
/* 1 parameter commands */
#define CMD_DISABLE_SEQ_NUM         0x01
#define CMD_ENABLE_SEQ_NUM          0x02
#define CMD_FILL_BUFFER_SEQ_NUM     0x03
#define CMD_CLEAR_BUFFER_SEQ_NUM    0x04
#define CMD_RELEASE_POLL_SEQ_NUM    0x05

#define ESS_MAGIC 'E'
#define IOCTL_DISABLE                 _IO(ESS_MAGIC, CMD_DISABLE_SEQ_NUM)
#define IOCTL_ENABLE                  _IO(ESS_MAGIC, CMD_ENABLE_SEQ_NUM)
#define IOCTL_FILL_BUFFER             _IO(ESS_MAGIC, CMD_FILL_BUFFER_SEQ_NUM)
#define IOCTL_CLEAR_BUFFER            _IO(ESS_MAGIC, CMD_CLEAR_BUFFER_SEQ_NUM)
#define IOCTL_RELEASE_POLL            _IO(ESS_MAGIC, CMD_RELEASE_POLL_SEQ_NUM)

/* write commands */
/* 2 parameter commands */
#define CMD_SET_PIXEL               0x05
#define CMD_GET_PIXEL               0x06
    #define PIXEL_X_IDX     0x01
    #define PIXEL_Y_IDX     0x02

/* 3 parameter commands */
#define CMD_H_LINE                  0x07
#define CMD_V_LINE                  0x08
    #define LINE_X_IDX      0x01
    #define LINE_Y_IDX      0x02
    #define LINE_LEN_IDX    0x03

/* 4 parameter commands */
#define CMD_RECT_FILL               0x09
#define CMD_RECT_CLEAR              0x0A
    #define RECT_X_IDX      0x01
    #define RECT_Y_IDX      0x02
    #define RECT_WIDTH      0x03
    #define RECT_HEIGHT     0x04

#ifdef __cplusplus
extern "C" {
#endif

/* button enumeration */
#define BUTTON_5 0
#define BUTTON_6 1
#define ROCKER_D 2
#define ROCKER_N 3
#define ROCKER_S 4
#define ROCKER_E 6
#define ROCKER_W 7

/* capture event metadata */
struct CaptureEvent {
    int button_num;         /* button number */
    uint64_t event;         /* incrementing event number */
};

/* capture event bulk data */
struct EventBulkData {
    struct CaptureEvent capture_event;      /* raw capture event data */
    uint64_t bulk_data[10];                 /* bulk data associated with capture event */
};

#ifdef __cplusplus
}
#endif