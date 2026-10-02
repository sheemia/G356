#include "g356_driver.h"
#include <string.h>

// ==========================================
// 辅助解析函数
// ==========================================
static inline int16_t parse_int16(const uint8_t *buf)
{
    // 小端序合并为 16 位有符号整数
    return (int16_t)(buf[0] | (buf[1] << 8));
}

static inline float parse_float(const uint8_t *buf)
{
    // 小端序合并为 32 位浮点数 (IEEE-754 标准)
    // 避免 ARM Cortex-M0+ 发生对齐异常，使用 memcpy 进行安全复制
    float val;
    memcpy(&val, buf, sizeof(float));
    return val;
}

// 校验一个已经凑满 G356_FRAME_SIZE 字节的窗口：类型/长度/校验和/帧尾
static bool G356_ValidateFrame(const uint8_t *frame_buf)
{
    // Type 固定为 0x02 表示姿态遥测数据，Length 固定为 0x36 (54字节，表示后续除了帧头帧尾的所有长度)
    uint8_t frame_size;
    if (frame_buf[2] == 0x02 && frame_buf[3] == 0x36) {
        frame_size = G356_LEGACY_FRAME_SIZE;
    } else if (frame_buf[2] == 0x04 && frame_buf[3] == 0x2E) {
        frame_size = G356_COMMON_FRAME_SIZE;
    } else if (frame_buf[2] == 0x03 && frame_buf[3] == 0x46) {
        frame_size = G356_QUATERNION_FRAME_SIZE;
    } else {
        return false;
    }

    // 帧尾必须是 0x5A
    if (frame_buf[frame_size - 1] != 0x5A) {
        return false;
    }

    // 和校验：从字节偏移 2 (Type) 累加到字节偏移 53 (Raw Gyro Z 最后一字节) 的所有字节之和
    uint8_t cal_checksum = 0;
    for (int i = 2; i <= frame_size - 3; i++) {
        cal_checksum += frame_buf[i];
    }
    if (cal_checksum != frame_buf[frame_size - 2]) {
        return false;
    }

    return true;
}

G356_FrameStatus G356_FeedByte(uint8_t byte, uint8_t *out_frame_buf)
{
    // 帧解析状态机：0=等待0xAA, 1=等待0x55(凑帧头), 2=定长收集剩余字节
    // 帧头探测只在状态0/1(空闲态)生效——UART 是连续字节流，没有 SPI 那种片选
    // 信号天然分帧，一旦进入状态2就只管定长收集，不会被payload里偶然出现的
    // 0xAA/0x55 提前打断，避免误同步。
    static uint8_t state = 0;
    static uint8_t buf[G356_MAX_FRAME_SIZE];
    static uint8_t idx = 0;
    static uint8_t expected_size = G356_COMMON_FRAME_SIZE;

    switch (state) {
        case 0:
            if (byte == 0xAA) {
                buf[0] = byte;
                state = 1;
            }
            break;

        case 1:
            if (byte == 0x55) {
                buf[1] = byte;
                idx = 2;
                state = 2;
            } else if (byte != 0xAA) {
                state = 0; // 不是合法帧头，回到空闲态重新搜索
            }
            // byte == 0xAA: 可能是新帧头的开始，停留在状态 1 继续等 0x55
            break;

        case 2:
            buf[idx++] = byte;
            if (idx == 4) {
                if (buf[2] == 0x02 && buf[3] == 0x36) {
                    expected_size = G356_LEGACY_FRAME_SIZE;
                } else if (buf[2] == 0x04 && buf[3] == 0x2E) {
                    expected_size = G356_COMMON_FRAME_SIZE;
                } else if (buf[2] == 0x03 && buf[3] == 0x46) {
                    expected_size = G356_QUATERNION_FRAME_SIZE;
                } else {
                    state = 0;
                    return G356_FRAME_INVALID;
                }
            }
            if (idx >= expected_size) {
                state = 0; // 无论校验是否通过，都回到空闲态准备下一帧
                if (G356_ValidateFrame(buf)) {
                    memcpy(out_frame_buf, buf, expected_size);
                    return G356_FRAME_VALID;
                }
                return G356_FRAME_INVALID;
            }
            break;

        default:
            state = 0;
            break;
    }

    return G356_FRAME_PENDING;
}

void G356_ParseData(const uint8_t *buf, G356_Data_t *data)
{
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
}
