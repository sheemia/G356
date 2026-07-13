#include "g356.h"

#include <string.h>

#include "g356_port.h"

static int16_t g356_parse_i16(const uint8_t *buf)
{
    return (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
}

static float g356_parse_float(const uint8_t *buf)
{
    float value;
    memcpy(&value, buf, sizeof(value));
    return value;
}

void G356_Init(G356_Handle_t *dev)
{
    if (dev == 0) {
        return;
    }

    memset(dev, 0, sizeof(*dev));
}

bool G356_ValidateFrame(const uint8_t *frame)
{
    uint8_t checksum = 0;
    uint16_t i;

    if (frame == 0) {
        return false;
    }
    if (frame[0] != G356_FRAME_HEADER0 || frame[1] != G356_FRAME_HEADER1) {
        return false;
    }
    if (frame[2] != G356_FRAME_TYPE_TELEMETRY || frame[3] != G356_FRAME_LENGTH) {
        return false;
    }
    if (frame[G356_FRAME_SIZE - 1u] != G356_FRAME_TAIL) {
        return false;
    }

    for (i = 2u; i <= (G356_FRAME_SIZE - 3u); i++) {
        checksum = (uint8_t)(checksum + frame[i]);
    }

    return checksum == frame[G356_FRAME_SIZE - 2u];
}

void G356_ParseFrame(const uint8_t *frame, G356_Data_t *data)
{
    if (frame == 0 || data == 0) {
        return;
    }

    data->accel_x = (float)g356_parse_i16(&frame[4]) * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_y = (float)g356_parse_i16(&frame[6]) * (1.0f / G356_ACCEL_LSB_PER_G);
    data->accel_z = (float)g356_parse_i16(&frame[8]) * (1.0f / G356_ACCEL_LSB_PER_G);

    data->gyro_x = (float)g356_parse_i16(&frame[10]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_y = (float)g356_parse_i16(&frame[12]) * (1.0f / G356_GYRO_LSB_PER_DPS);
    data->gyro_z = (float)g356_parse_i16(&frame[14]) * (1.0f / G356_GYRO_LSB_PER_DPS);

    data->roll = g356_parse_float(&frame[16]);
    data->pitch = g356_parse_float(&frame[20]);
    data->yaw = g356_parse_float(&frame[24]);
    data->temp = (float)g356_parse_i16(&frame[28]) * (1.0f / G356_TEMP_LSB_PER_DEGC);

    data->raw_accel_x = g356_parse_float(&frame[30]);
    data->raw_accel_y = g356_parse_float(&frame[34]);
    data->raw_accel_z = g356_parse_float(&frame[38]);
    data->raw_gyro_x = g356_parse_float(&frame[42]);
    data->raw_gyro_y = g356_parse_float(&frame[46]);
    data->raw_gyro_z = g356_parse_float(&frame[50]);
}

G356_Result_t G356_FeedByte(G356_Handle_t *dev, uint8_t byte)
{
    if (dev == 0) {
        return G356_RESULT_PORT_ERROR;
    }

    switch (dev->parser_state) {
        case 0:
            if (byte == G356_FRAME_HEADER0) {
                dev->parser_buf[0] = byte;
                dev->parser_state = 1;
            }
            break;

        case 1:
            if (byte == G356_FRAME_HEADER1) {
                dev->parser_buf[1] = byte;
                dev->parser_index = 2;
                dev->parser_state = 2;
            } else if (byte == G356_FRAME_HEADER0) {
                dev->parser_buf[0] = byte;
            } else {
                dev->parser_state = 0;
            }
            break;

        case 2:
            dev->parser_buf[dev->parser_index++] = byte;
            if (dev->parser_index >= G356_FRAME_SIZE) {
                dev->parser_state = 0;
                dev->parser_index = 0;

                if (G356_ValidateFrame(dev->parser_buf)) {
                    memcpy(dev->frame, dev->parser_buf, G356_FRAME_SIZE);
                    G356_ParseFrame(dev->frame, &dev->data);
                    dev->valid_count++;
                    return G356_RESULT_OK;
                }

                dev->invalid_count++;
                return G356_RESULT_BAD_FRAME;
            }
            break;

        default:
            dev->parser_state = 0;
            dev->parser_index = 0;
            break;
    }

    return G356_RESULT_NONE;
}

G356_Result_t G356_Update(G356_Handle_t *dev)
{
#if G356_USE_UART
    uint16_t count;
    uint8_t byte;

    for (count = 0; count < G356_UART_MAX_BYTES_PER_UPDATE; count++) {
        int ret = G356_PortUartReadByte(&byte);
        G356_Result_t result;

        if (ret < 0) {
            return G356_RESULT_PORT_ERROR;
        }
        if (ret == 0) {
            break;
        }

        result = G356_FeedByte(dev, byte);
        if (result == G356_RESULT_OK || result == G356_RESULT_BAD_FRAME) {
            return result;
        }
    }
#endif

#if G356_USE_SPI
    if (dev == 0) {
        return G356_RESULT_PORT_ERROR;
    }
    if (G356_PortSpiReadFrame(dev->frame, G356_FRAME_SIZE) < 0) {
        return G356_RESULT_PORT_ERROR;
    }
    if (G356_ValidateFrame(dev->frame)) {
        G356_ParseFrame(dev->frame, &dev->data);
        dev->valid_count++;
        return G356_RESULT_OK;
    }
    dev->invalid_count++;
    return G356_RESULT_BAD_FRAME;
#endif

    return G356_RESULT_NONE;
}

uint16_t G356_Crc16Xmodem(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0x0000u;
    uint16_t i;

    for (i = 0; i < len; i++) {
        uint8_t bit;
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8u; bit++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

void G356_BuildFixedCommand(uint8_t cmd, uint8_t param, uint8_t out_cmd[G356_FIXED_CMD_SIZE])
{
    uint8_t crc_input[2];
    uint16_t crc;

    if (out_cmd == 0) {
        return;
    }

    crc_input[0] = cmd;
    crc_input[1] = param;
    crc = G356_Crc16Xmodem(crc_input, 2);

    out_cmd[0] = 0xAAu;
    out_cmd[1] = 0xAFu;
    out_cmd[2] = cmd;
    out_cmd[3] = param;
    out_cmd[4] = (uint8_t)(crc & 0xFFu);
    out_cmd[5] = (uint8_t)(crc >> 8);
    out_cmd[6] = 0x5Au;
}

#if G356_ENABLE_DOWNLINK
G356_Result_t G356_SendCommand(uint8_t cmd, uint8_t param)
{
    uint8_t packet[G356_FIXED_CMD_SIZE];

    G356_BuildFixedCommand(cmd, param, packet);
    if (G356_PortUartWrite(packet, G356_FIXED_CMD_SIZE) < 0) {
        return G356_RESULT_PORT_ERROR;
    }

    return G356_RESULT_OK;
}

G356_Result_t G356_SendCommandRepeated(uint8_t cmd, uint8_t param)
{
    uint8_t i;

    for (i = 0; i < G356_COMMAND_REPEAT_COUNT; i++) {
        G356_Result_t result = G356_SendCommand(cmd, param);
        if (result != G356_RESULT_OK) {
            return result;
        }
        if ((i + 1u) < G356_COMMAND_REPEAT_COUNT) {
            G356_PortDelayMs(G356_COMMAND_REPEAT_DELAY_MS);
        }
    }

    return G356_RESULT_OK;
}
#endif
