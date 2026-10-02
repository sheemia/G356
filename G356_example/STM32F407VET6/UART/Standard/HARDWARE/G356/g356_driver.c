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
    uint8_t frame_size;
    uint32_t i;

    if (buf[0] != 0xAA || buf[1] != 0x55) {
        return 0;
    }
    if (buf[2] == 0x02 && buf[3] == 0x36) frame_size = G356_LEGACY_FRAME_SIZE;
    else if (buf[2] == 0x04 && buf[3] == 0x2E) frame_size = G356_COMMON_FRAME_SIZE;
    else if (buf[2] == 0x03 && buf[3] == 0x46) frame_size = G356_QUATERNION_FRAME_SIZE;
    else return 0;
    if (buf[frame_size - 1u] != 0x5A) return 0;

    for (i = 2; i <= frame_size - 3u; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }

    return (sum == buf[frame_size - 2u]);
}

G356_FrameStatus G356_FeedByte(uint8_t byte, uint8_t *frame_buf)
{
    static uint8_t idx = 0;
    static uint8_t expected_size = G356_COMMON_FRAME_SIZE;

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

    if (idx == 4u) {
        if (frame_buf[2] == 0x02 && frame_buf[3] == 0x36) expected_size = G356_LEGACY_FRAME_SIZE;
        else if (frame_buf[2] == 0x04 && frame_buf[3] == 0x2E) expected_size = G356_COMMON_FRAME_SIZE;
        else if (frame_buf[2] == 0x03 && frame_buf[3] == 0x46) expected_size = G356_QUATERNION_FRAME_SIZE;
        else {
            idx = 0;
            return G356_FRAME_INVALID;
        }
    }

    if (idx >= expected_size) {
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

    const bool common = (buf[2] == 0x04);
    const bool legacy = (buf[2] == 0x02);
    memset(data, 0, sizeof(*data));
    // 1. 解析加速度计原始数据，乘以分度值的倒数转换为 g (编译期常量折叠，无运行时除法开销)
    data->accel_x = (float)parse_int16(&buf[4])  * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_y = (float)parse_int16(&buf[6])  * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_z = (float)parse_int16(&buf[8])  * (1.0f / G356_ACCEL_LSB_PER_G);

    // 2. 解析陀螺仪原始数据，转换为 dps (度/秒)
    data->gyro_x = (float)parse_int16(&buf[10]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_y = (float)parse_int16(&buf[12]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_z = (float)parse_int16(&buf[14]) * (1.0f / G356_GYRO_LSB_PER_DPS);

    // 3. 解析欧拉角 (Roll, Pitch, Yaw)
    // 原数据即为小端序标准 32 位浮点数 (deg)
    // 旧帧保留历史 Pitch/Roll 顺序；扩展帧使用实际 Roll/Pitch 顺序。
    data->roll  = parse_float(&buf[legacy ? 20 : 16]);
    data->pitch = parse_float(&buf[legacy ? 16 : 20]);
    data->yaw   = parse_float(&buf[24]);

    // 4. 解析温度数据，转换为 ℃
    data->temp = (float)parse_int16(&buf[28]) * (1.0f / G356_TEMP_LSB_PER_DEGC);

    // 5. 解析未量化、未扣校准offset的原始浮点六轴数据 (标定/温度补偿数据采集用)
    if (!common) {
        data->raw_accel_x = parse_float(&buf[30]);
        data->raw_accel_y = parse_float(&buf[34]);
        data->raw_accel_z = parse_float(&buf[38]);
        data->raw_gyro_x  = parse_float(&buf[42]);
        data->raw_gyro_y  = parse_float(&buf[46]);
        data->raw_gyro_z  = parse_float(&buf[50]);
    }
    data->has_quaternion = common || (buf[2] == 0x03 && buf[3] == 0x46);
    if (data->has_quaternion) {
        const uint16_t offset = common ? 30u : 54u;
        data->quat_w = parse_float(&buf[offset]);
        data->quat_x = parse_float(&buf[offset + 4u]);
        data->quat_y = parse_float(&buf[offset + 8u]);
        data->quat_z = parse_float(&buf[offset + 12u]);
    } else {
        data->quat_w = 1.0f;
        data->quat_x = 0.0f;
        data->quat_y = 0.0f;
        data->quat_z = 0.0f;
    }

    return 1;
}
