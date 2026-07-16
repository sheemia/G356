#include "g356_driver.h"
#include "spi.h"
#include <string.h>

// ==========================================
// 辅助解析函数
// ==========================================
static int16_t parse_int16(const u8 *buf)
{
    // 小端序合并为 16 位有符号整数
    return (int16_t)(buf[0] | (buf[1] << 8));
}

static float parse_float(const u8 *buf)
{
    // 小端序合并为 32 位浮点数 (IEEE-754 标准)
    // 避免 Cortex-M3 非对齐访问问题，使用 memcpy 安全复制
    float val;
    memcpy(&val, buf, sizeof(float));
    return val;
}

u8 G356_ReadPacket(u8 *frame_buf)
{
    int i;

    // 1. 拉低片选信号 (CS Pin)，开始 SPI 事务
    G356_CS_LOW();

    // 2. 短暂延时，给从机充裕的时间检测 CS 拉低并准备移位寄存器
    for (i = 0; i < 50; i++) { __NOP(); }

    // 3. 全双工逐字节突发读取 56 字节 (CS 全程保持低电平)
    for (i = 0; i < G356_FRAME_SIZE; i++) {
        frame_buf[i] = SPI2_ReadWriteByte(0xFF);
    }

    // 4. 短暂延时，确保最后一个字节的所有 SCLK 时钟沿完全就绪后，再释放片选
    for (i = 0; i < 50; i++) { __NOP(); }

    // 拉高片选信号 (CS Pin)，结束 SPI 事务
    G356_CS_HIGH();

    // 5. 校验数据帧头 (Header)
    if (frame_buf[0] != 0xAA || frame_buf[1] != 0x55) {
        return 0; // 帧头不正确，数据包丢弃
    }

    // 6. 校验数据帧尾 (Tail)
    if (frame_buf[G356_FRAME_SIZE - 1] != 0x5A) {
        return 0; // 帧尾不正确，数据包丢弃
    }

    // 7. 校验数据类型 (Type) 与 载荷长度 (Length)
    // Type 固定为 0x02 表示姿态遥测数据，Length 固定为 0x36 (54字节)
    if (frame_buf[2] != 0x02 || frame_buf[3] != 0x36) {
        return 0;
    }

    // 8. 计算和校验 (Checksum)
    // 校验规则：从字节偏移 2 (Type) 累加到字节偏移 53 (Raw Gyro Z 最后一字节) 的所有字节之和
    {
        u8 cal_checksum = 0;
        for (i = 2; i <= G356_FRAME_SIZE - 3; i++) {
            cal_checksum += frame_buf[i];
        }
        // 比较计算出的校验和与接收到的校验和 (字节偏移 30)
        if (cal_checksum != frame_buf[G356_FRAME_SIZE - 2]) {
            return 0; // 校验和不匹配，数据已损坏
        }
    }

    return 1; // 校验通过，数据包有效
}

void G356_ParseData(const u8 *buf, G356_Data_t *data)
{
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
    data->roll  = parse_float(&buf[16]);
    data->pitch = parse_float(&buf[20]);
    data->yaw   = parse_float(&buf[24]);

    // 4. 解析温度数据，转换为 ℃
    data->temp = (float)parse_int16(&buf[28]) * (1.0f / G356_TEMP_LSB_PER_DEGC);

    // 5. 解析未量化、未扣校准offset的原始浮点六轴数据 (标定/温度补偿数据采集用)
    data->raw_accel_x = parse_float(&buf[30]);
    data->raw_accel_y = parse_float(&buf[34]);
    data->raw_accel_z = parse_float(&buf[38]);
    data->raw_gyro_x  = parse_float(&buf[42]);
    data->raw_gyro_y  = parse_float(&buf[46]);
    data->raw_gyro_z  = parse_float(&buf[50]);
}
