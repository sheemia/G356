#include "g356_driver.h"
#include <string.h>

static int16_t parse_int16(const uint8_t *buf)
{
    return (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

static float parse_float(const uint8_t *buf)
{
    float val;
    memcpy(&val, buf, sizeof(val));
    return val;
}

static uint8_t checksum_ok(const uint8_t *buf)
{
    uint8_t sum = 0;
    uint32_t i;

    if (buf[0] != 0xAA || buf[1] != 0x55 || buf[2] != 0x02 || buf[3] != 0x36 || buf[55] != 0x5A) {
        return 0;
    }

    for (i = 2; i <= 53; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }

    return (sum == buf[54]);
}

G356_FrameStatus G356_FeedByte(uint8_t byte, uint8_t *frame_buf)
{
    static uint8_t idx = 0;

    if (idx == 0) {
        if (byte != 0xAA) {
            return G356_FRAME_PENDING;
        }
        frame_buf[idx++] = byte;
        return G356_FRAME_PENDING;
    }

    if (idx == 1) {
        if (byte != 0x55) {
            idx = (byte == 0xAA) ? 1 : 0;
            frame_buf[0] = 0xAA;
            return G356_FRAME_PENDING;
        }
        frame_buf[idx++] = byte;
        return G356_FRAME_PENDING;
    }

    frame_buf[idx++] = byte;

    if (idx >= G356_FRAME_LEN) {
        idx = 0;
        return checksum_ok(frame_buf) ? G356_FRAME_VALID : G356_FRAME_INVALID;
    }

    return G356_FRAME_PENDING;
}

uint8_t G356_ParseData(const uint8_t *buf, G356_Data_t *data)
{
    if (!checksum_ok(buf) || data == 0) {
        return 0;
    }

    data->accel_x = (float)parse_int16(&buf[4]) * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_y = (float)parse_int16(&buf[6]) * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_z = (float)parse_int16(&buf[8]) * (1.0f / G356_ACCEL_LSB_PER_G);

    data->gyro_x = (float)parse_int16(&buf[10]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_y = (float)parse_int16(&buf[12]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_z = (float)parse_int16(&buf[14]) * (1.0f / G356_GYRO_LSB_PER_DPS);

    data->roll  = parse_float(&buf[16]);
    data->pitch = parse_float(&buf[20]);
    data->yaw   = parse_float(&buf[24]);
    data->temp  = (float)parse_int16(&buf[28]) * (1.0f / G356_TEMP_LSB_PER_DEGC);

    data->raw_accel_x = parse_float(&buf[30]);
    data->raw_accel_y = parse_float(&buf[34]);
    data->raw_accel_z = parse_float(&buf[38]);
    data->raw_gyro_x  = parse_float(&buf[42]);
    data->raw_gyro_y  = parse_float(&buf[46]);
    data->raw_gyro_z  = parse_float(&buf[50]);

    return 1;
}
