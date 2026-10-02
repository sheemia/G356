#ifndef __G356_DRIVER_H
#define __G356_DRIVER_H

#include "stm32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

#define G356_LEGACY_FRAME_SIZE      56u
#define G356_COMMON_FRAME_SIZE      48u
#define G356_QUATERNION_FRAME_SIZE  72u
#define G356_MAX_FRAME_SIZE         G356_QUATERNION_FRAME_SIZE
#define G356_FRAME_LEN              G356_COMMON_FRAME_SIZE
#define G356_GYRO_LSB_PER_DPS       (8.2f)
#define G356_ACCEL_LSB_PER_G        (2048.0f)
#define G356_TEMP_LSB_PER_DEGC      (100.0f)

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float roll;
    float pitch;
    float yaw;
    float temp;
    float raw_accel_x;
    float raw_accel_y;
    float raw_accel_z;
    float raw_gyro_x;
    float raw_gyro_y;
    float raw_gyro_z;
    float quat_w;
    float quat_x;
    float quat_y;
    float quat_z;
    uint8_t has_quaternion;
} G356_Data_t;

typedef enum {
    G356_FRAME_PENDING = 0,
    G356_FRAME_VALID,
    G356_FRAME_INVALID
} G356_FrameStatus;

G356_FrameStatus G356_FeedByte(uint8_t byte, uint8_t *frame_buf);
uint8_t G356_ParseData(const uint8_t *buf, G356_Data_t *data);

#endif
