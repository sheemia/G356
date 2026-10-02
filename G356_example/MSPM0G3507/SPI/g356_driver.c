#include "g356_driver.h"
#include <string.h>
#include "ti_msp_dl_config.h"

// ==========================================
// SPI 读写单字节：全双工通信
// ==========================================
static uint8_t SPI_ReadWriteByte(uint8_t tx_data)
{
    // 1. 等待 SPI 发送 FIFO 有空闲空间，防止溢出
    while (DL_SPI_isTXFIFOFull(SPI_G356_INST)) {
        // 忙等待
    }

    // 2. 将数据写入发送 FIFO (如果是纯读取，可写入 0xFF 产生时钟)
    DL_SPI_transmitData8(SPI_G356_INST, tx_data);

    // 3. 等待接收 FIFO 非空，确认硬件已完成这 1 字节的全双工交换
    while (DL_SPI_isRXFIFOEmpty(SPI_G356_INST)) {
        // 忙等待
    }

    // 4. 从接收 FIFO 中读取并返回交换回来的数据
    return DL_SPI_receiveData8(SPI_G356_INST);
}

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

static bool G356_ReadPacketSized(uint8_t *frame_buf, uint8_t expected_size,
                                 uint8_t *actual_size)
{
    uint8_t frame_size = expected_size;
    uint8_t checksum = 0;
    int i;

    while (!DL_SPI_isRXFIFOEmpty(SPI_G356_INST)) {
        (void)DL_SPI_receiveData8(SPI_G356_INST);
    }
    DL_GPIO_clearPins(GPIO_CS_PORT, GPIO_CS_CS_PIN_PIN);
    delay_cycles(3200); /* Allow the G356 CS interrupt to load its TX FIFO. */

    if (expected_size == 0u) {
        for (i = 0; i < 4; i++) frame_buf[i] = SPI_ReadWriteByte(0xFF);
        if (frame_buf[0] == 0xAA && frame_buf[1] == 0x55) {
            frame_size = (uint8_t)(frame_buf[3] + 2u);
        }
        if (frame_size < 6u || frame_size > G356_MAX_FRAME_SIZE) {
            /* Drain a complete transfer after a bad header to resynchronize. */
            for (i = 4; i < G356_MAX_FRAME_SIZE; i++) (void)SPI_ReadWriteByte(0xFF);
            DL_GPIO_setPins(GPIO_CS_PORT, GPIO_CS_CS_PIN_PIN);
            return false;
        }
        for (i = 4; i < frame_size; i++) frame_buf[i] = SPI_ReadWriteByte(0xFF);
    } else {
        for (i = 0; i < frame_size; i++) frame_buf[i] = SPI_ReadWriteByte(0xFF);
    }
    delay_cycles(160);
    DL_GPIO_setPins(GPIO_CS_PORT, GPIO_CS_CS_PIN_PIN);

    if (frame_buf[0] != 0xAA || frame_buf[1] != 0x55 ||
        frame_buf[frame_size - 1u] != 0x5A ||
        !((frame_size == G356_COMMON_FRAME_SIZE && frame_buf[2] == 0x04 && frame_buf[3] == 0x2E) ||
          (frame_size == G356_LEGACY_FRAME_SIZE && frame_buf[2] == 0x02 && frame_buf[3] == 0x36) ||
          (frame_size == G356_QUATERNION_FRAME_SIZE && frame_buf[2] == 0x03 && frame_buf[3] == 0x46))) {
        return false;
    }
    for (i = 2; i < frame_size - 2; i++) checksum += frame_buf[i];
    if (checksum != frame_buf[frame_size - 2u]) return false;
    if (actual_size != NULL) *actual_size = frame_size;
    return true;
}

bool G356_ReadPacket(uint8_t *frame_buf)
{
    return G356_ReadPacketSized(frame_buf, G356_LEGACY_FRAME_SIZE, NULL);
}

bool G356_ReadQuaternionPacket(uint8_t *frame_buf)
{
    return G356_ReadPacketSized(frame_buf, G356_QUATERNION_FRAME_SIZE, NULL);
}

bool G356_ReadFrame(uint8_t *frame_buf, uint8_t *frame_size)
{
    return frame_size != NULL && G356_ReadPacketSized(frame_buf, 0u, frame_size);
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
