#include "g356_driver.h"
#include <string.h>
#include "main.h"
#include "spi.h"

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
    // Cortex-M4 支持非对齐访问，这里仍用 memcpy 保持跨平台一致性
    float val;
    memcpy(&val, buf, sizeof(float));
    return val;
}

static bool G356_ReadPacketSized(uint8_t *frame_buf, uint16_t expected_size,
                                 uint16_t *actual_size)
{
    uint8_t tx_dummy[G356_MAX_FRAME_SIZE];
    uint16_t frame_size = expected_size;
    HAL_StatusTypeDef status;
    memset(tx_dummy, 0xFF, sizeof(tx_dummy));

    // 1. 拉低片选信号 (CS Pin)，开始 SPI 事务
    HAL_GPIO_WritePin(GPIO_CS_GPIO_Port, GPIO_CS_Pin, GPIO_PIN_RESET);

    // 2. G356 使用 GPIO 中断检测 CS；等待其完成帧快照和 TX FIFO 预装载。
    // HAL_Delay(2) 在任意 SysTick 相位下都能保证至少 1 ms 的准备时间。
    HAL_Delay(2);

    // 3. 自动模式先读取 4 字节帧头，再按 Length 读取余下字节；CS 始终保持低电平。
    if (expected_size == 0u) {
        status = HAL_SPI_TransmitReceive(&hspi1, tx_dummy, frame_buf, 4, 100);
        if (status == HAL_OK && frame_buf[0] == 0xAA && frame_buf[1] == 0x55) {
            frame_size = (uint16_t)frame_buf[3] + 2u;
            if (frame_size < 6u || frame_size > G356_MAX_FRAME_SIZE) {
                status = HAL_ERROR;
            } else {
                status = HAL_SPI_TransmitReceive(&hspi1, tx_dummy + 4,
                                                 frame_buf + 4, frame_size - 4u, 100);
            }
        }
        if (status == HAL_OK && (frame_buf[0] != 0xAA || frame_buf[1] != 0x55)) {
            status = HAL_ERROR;
        }
        if (status == HAL_ERROR) {
            // 帧头错位时继续给时钟，清空本次事务的余字节，下一次 CS 从帧头重试。
            (void)HAL_SPI_TransmitReceive(&hspi1, tx_dummy + 4, frame_buf + 4,
                                          G356_MAX_FRAME_SIZE - 4u, 100);
        }
    } else {
        status = HAL_SPI_TransmitReceive(&hspi1, tx_dummy, frame_buf, frame_size, 100);
    }

    // 4. 短暂延时，确保最后一个字节的所有 SCLK 时钟沿完全就绪后，再释放片选
    for (volatile int i = 0; i < 50; i++) { __NOP(); }

    // 拉高片选信号 (CS Pin)，结束 SPI 事务
    HAL_GPIO_WritePin(GPIO_CS_GPIO_Port, GPIO_CS_Pin, GPIO_PIN_SET);

    if (status != HAL_OK || frame_size < 6u) {
        return false;
    }

    // 5. 校验数据帧头 (Header)
    if (frame_buf[0] != 0xAA || frame_buf[1] != 0x55) {
        return false; // 帧头不正确，数据包丢弃
    }

    // 6. 校验数据帧尾 (Tail)
    if (frame_buf[frame_size - 1] != 0x5A) {
        return false; // 帧尾不正确，数据包丢弃
    }

    // 7. 校验数据类型 (Type) 与 载荷长度 (Length)
    // Type 固定为 0x02 表示姿态遥测数据，Length 固定为 0x36 (54字节)
    if (!((frame_size == G356_LEGACY_FRAME_SIZE && frame_buf[2] == 0x02 && frame_buf[3] == 0x36) ||
          (frame_size == G356_COMMON_FRAME_SIZE && frame_buf[2] == 0x04 && frame_buf[3] == 0x2E) ||
          (frame_size == G356_QUATERNION_FRAME_SIZE && frame_buf[2] == 0x03 && frame_buf[3] == 0x46))) {
        return false;
    }

    // 8. 计算和校验 (Checksum)
    // 校验规则：从字节偏移 2 (Type) 累加到字节偏移 53 (Raw Gyro Z 最后一字节) 的所有字节之和
    uint8_t cal_checksum = 0;
    for (int i = 2; i <= frame_size - 3; i++) {
        cal_checksum += frame_buf[i];
    }

    // 比较计算出的校验和与接收到的校验和 (字节偏移 30)
    if (cal_checksum != frame_buf[frame_size - 2]) {
        return false; // 校验和不匹配，数据已损坏
    }

    if (actual_size != NULL) {
        *actual_size = frame_size;
    }
    return true; // 校验通过，数据包有效
}

bool G356_ReadPacket(uint8_t *frame_buf)
{
    return G356_ReadPacketSized(frame_buf, G356_LEGACY_FRAME_SIZE, NULL);
}

bool G356_ReadQuaternionPacket(uint8_t *frame_buf)
{
    return G356_ReadPacketSized(frame_buf, G356_QUATERNION_FRAME_SIZE, NULL);
}

bool G356_ReadFrame(uint8_t *frame_buf, uint16_t *frame_size)
{
    if (frame_size == NULL) {
        return false;
    }
    return G356_ReadPacketSized(frame_buf, 0u, frame_size);
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
