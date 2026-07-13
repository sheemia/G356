#ifndef G356_H
#define G356_H

#include <stdbool.h>
#include <stdint.h>

#include "g356_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G356_FRAME_SIZE              56u
#define G356_FIXED_CMD_SIZE           7u

#define G356_FRAME_HEADER0         0xAAu
#define G356_FRAME_HEADER1         0x55u
#define G356_FRAME_TYPE_TELEMETRY  0x02u
#define G356_FRAME_LENGTH          0x36u
#define G356_FRAME_TAIL            0x5Au

#define G356_ACCEL_LSB_PER_G       2048.0f
#define G356_GYRO_LSB_PER_DPS      8.2f
#define G356_TEMP_LSB_PER_DEGC     100.0f

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
} G356_Data_t;

typedef enum {
    G356_RESULT_NONE = 0,
    G356_RESULT_OK,
    G356_RESULT_BAD_FRAME,
    G356_RESULT_PORT_ERROR,
} G356_Result_t;

typedef enum {
    G356_CMD_GYRO_CALIBRATE      = 0x10,
    G356_CMD_ACCEL_LEVEL_CALIB   = 0x11,
    G356_CMD_SENSOR_RESET        = 0x12,
    G356_CMD_SET_OUTPUT_CHANNEL  = 0x14,
    G356_CMD_SET_BAUDRATE        = 0x16,
    G356_CMD_ZERO_YAW            = 0x18,
    G356_CMD_RECOVER_115200      = 0x19,
    G356_CMD_RESTORE_FACTORY     = 0x1E,
    G356_CMD_SET_ODR_DIVIDER     = 0x20,
} G356_Command_t;

typedef enum {
    G356_BAUD_1200    = 0x00,
    G356_BAUD_2400    = 0x01,
    G356_BAUD_4800    = 0x02,
    G356_BAUD_9600    = 0x03,
    G356_BAUD_19200   = 0x04,
    G356_BAUD_38400   = 0x05,
    G356_BAUD_57600   = 0x06,
    G356_BAUD_115200  = 0x07,
    G356_BAUD_230400  = 0x08,
    G356_BAUD_460800  = 0x09,
    G356_BAUD_921600  = 0x0A,
    G356_BAUD_1000000 = 0x0B,
    G356_BAUD_1500000 = 0x0C,
    G356_BAUD_2000000 = 0x0D,
    G356_BAUD_3000000 = 0x0E,
    G356_BAUD_4000000 = 0x0F,
    G356_BAUD_5000000 = 0x10,
} G356_BaudCode_t;

typedef enum {
    G356_CHANNEL_UART = 0x01,
    G356_CHANNEL_SPI  = 0x02,
    G356_CHANNEL_BOTH = 0x03,
} G356_Channel_t;

typedef enum {
    G356_ODR_800HZ = 1,
    G356_ODR_400HZ = 2,
    G356_ODR_200HZ = 4,
    G356_ODR_100HZ = 8,
} G356_OdrDivider_t;

typedef struct {
    G356_Data_t data;
    uint8_t frame[G356_FRAME_SIZE];
    uint32_t valid_count;
    uint32_t invalid_count;
    uint8_t parser_state;
    uint8_t parser_index;
    uint8_t parser_buf[G356_FRAME_SIZE];
} G356_Handle_t;

void G356_Init(G356_Handle_t *dev);
G356_Result_t G356_Update(G356_Handle_t *dev);
G356_Result_t G356_FeedByte(G356_Handle_t *dev, uint8_t byte);

bool G356_ValidateFrame(const uint8_t *frame);
void G356_ParseFrame(const uint8_t *frame, G356_Data_t *data);

uint16_t G356_Crc16Xmodem(const uint8_t *data, uint16_t len);
void G356_BuildFixedCommand(uint8_t cmd, uint8_t param, uint8_t out_cmd[G356_FIXED_CMD_SIZE]);

#if G356_ENABLE_DOWNLINK
G356_Result_t G356_SendCommand(uint8_t cmd, uint8_t param);
G356_Result_t G356_SendCommandRepeated(uint8_t cmd, uint8_t param);
#endif

#ifdef __cplusplus
}
#endif

#endif /* G356_H */
