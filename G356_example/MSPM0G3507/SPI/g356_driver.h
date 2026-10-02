#ifndef G356_DRIVER_H
#define G356_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

// ==========================================================================
// G356 AHRS Module - SPI Driver (Controller / Master side)
//
// Wiring (G356 module  ->  your MCU's SPI Controller peripheral):
//   G356 VCC   -> 3.3V                  (module power, must be stable)
//   G356 GND   -> GND                   (must share ground with the MCU)
//   G356 CS    -> any GPIO              (manual chip-select, active low)
//   G356 SCLK  -> SPI SCLK              (Mode 0: CPOL=0, CPHA=0)
//   G356 MISO  -> SPI Controller MISO   (G356 -> MCU, telemetry data)
//   G356 MOSI  -> SPI Controller MOSI   (G356 only transmits; this line can
//                                        be left unconnected on real hardware)
//
// Reference clock: 2 MHz, 8-bit words, MSB first. See ../README.md for the
// full pin table and the 56-byte telemetry frame layout.
//
// Usage in your own project:
//   1. Copy g356_driver.h/.c into your project as-is.
//   2. Implement SPI_ReadWriteByte()'s equivalent for your own SPI peripheral
//      if you're not using TI DriverLib, or adjust the SPI_G356_INST /
//      GPIO_CS_* macro names below to match your SysConfig instance names.
//   3. Call G356_ReadFrame() to get one validated 48/56/72-byte frame, then
//      G356_ParseData() to convert it to engineering units.
// ==========================================================================

#define G356_LEGACY_FRAME_SIZE     56u
#define G356_COMMON_FRAME_SIZE     48u
#define G356_QUATERNION_FRAME_SIZE 72u
#define G356_MAX_FRAME_SIZE        G356_QUATERNION_FRAME_SIZE
/* Backward-compatible alias: existing applications remain 56-byte by default. */
#define G356_FRAME_SIZE            G356_LEGACY_FRAME_SIZE

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
    float quat_w;     // Quaternion scalar component (extended frame only)
    float quat_x;
    float quat_y;
    float quat_z;
    bool has_quaternion;
} G356_Data_t;

// Reads one 56-byte SPI frame from the G356 module and validates its header,
// type/length fields, checksum, and tail byte.
// Returns true and fills frame_buf (G356_FRAME_SIZE bytes) on success; returns
// false if the frame failed validation (caller decides whether to retry).
bool G356_ReadPacket(uint8_t *frame_buf);
bool G356_ReadQuaternionPacket(uint8_t *frame_buf);
/* Read current 48/56/72-byte firmware format. Buffer must hold 72 bytes. */
bool G356_ReadFrame(uint8_t *frame_buf, uint8_t *frame_size);

// Parses an already-validated G356_FRAME_SIZE-byte buffer (as returned by
// G356_ReadPacket) into engineering units.
void G356_ParseData(const uint8_t *buf, G356_Data_t *data);

#endif /* G356_DRIVER_H */
