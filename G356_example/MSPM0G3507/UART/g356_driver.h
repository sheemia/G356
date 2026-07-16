#ifndef G356_DRIVER_H
#define G356_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// ==========================================================================
// G356 AHRS Module - UART Driver (receiver side)
//
// Wiring (G356 module  ->  your MCU's UART peripheral):
//   G356 VCC   -> 3.3V                  (module power, must be stable)
//   G356 GND   -> GND                   (must share ground with the MCU)
//   G356 TXD   -> your UART's RX pin    (G356 -> MCU, telemetry data)
//   G356 RXD   -> your UART's TX pin    (only needed if you also send
//                                        downlink commands to G356)
//
// Reference config: 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).
// See ../README.md for the full pin table and the 56-byte telemetry frame
// layout. Unlike the SPI variant, there is no chip-select to frame a
// transfer, so this driver is a byte-stream state machine: feed it every
// byte you receive and it tells you when a complete, validated frame is
// ready.
//
// Usage in your own project:
//   1. Copy g356_driver.h/.c into your project as-is.
//   2. In your UART RX path (polling loop or RX interrupt), call
//      G356_FeedByte() with each received byte.
//   3. When it returns G356_FRAME_VALID, call G356_ParseData() on the
//      returned buffer to get engineering units.
// ==========================================================================

#define G356_FRAME_SIZE 56

// Raw-to-engineering-unit scale factors for the default sensor configuration
// (Accel +/-16g, Gyro +/-4000dps). If you ever change G356's FSR settings,
// update these to match.
#define G356_ACCEL_LSB_PER_G    (2048.0f)
#define G356_GYRO_LSB_PER_DPS   (8.2f)
#define G356_TEMP_LSB_PER_DEGC  (100.0f)

// Parsed telemetry data, in engineering units.
typedef struct {
    float accel_x;    // 加速度计 X 轴 (单位: g)
    float accel_y;    // 加速度计 Y 轴 (单位: g)
    float accel_z;    // 加速度计 Z 轴 (单位: g)
    float gyro_x;     // 陀螺仪 X 轴 (单位: dps, 度/秒)
    float gyro_y;     // 陀螺仪 Y 轴 (单位: dps, 度/秒)
    float gyro_z;     // 陀螺仪 Z 轴 (单位: dps, 度/秒)
    float roll;       // 横滚角 Roll (单位: 度, 范围 -180 ~ +180)
    float pitch;      // 俯仰角 Pitch (单位: 度, 范围 -90 ~ +90)
    float yaw;        // 偏航角 Yaw (单位: 度, 范围 -180 ~ +180)
    float temp;       // 模块内部温度 (单位: ℃)
    float raw_accel_x; // 未量化、未扣校准offset的加速度计原始浮点值 X (单位: g)
    float raw_accel_y; // 同上 Y
    float raw_accel_z; // 同上 Z
    float raw_gyro_x;  // 未量化、未扣校准offset的陀螺仪原始浮点值 X (单位: dps)
    float raw_gyro_y;  // 同上 Y
    float raw_gyro_z;  // 同上 Z
} G356_Data_t;

typedef enum {
    G356_FRAME_PENDING = 0, // 还没收完一整帧，继续喂下一个字节
    G356_FRAME_VALID,       // out_frame_buf 已经是一个校验通过的完整 56 字节帧
    G356_FRAME_INVALID,     // 凑够 56 字节但校验失败(帧头/类型/校验和/帧尾不对)，已自动重新搜索帧头
} G356_FrameStatus;

// Feeds one received UART byte into the frame parser state machine.
// Call this once per byte from your UART RX path (polling or interrupt).
// When this returns G356_FRAME_VALID, out_frame_buf contains a validated
// G356_FRAME_SIZE-byte frame ready for G356_ParseData().
G356_FrameStatus G356_FeedByte(uint8_t byte, uint8_t *out_frame_buf);

// Parses an already-validated G356_FRAME_SIZE-byte buffer (as returned by
// G356_FeedByte) into engineering units.
void G356_ParseData(const uint8_t *buf, G356_Data_t *data);

#endif /* G356_DRIVER_H */
