/*
  helpers.h -- minimal VESC helpers (renamed from refactored_helpers.h)
*/

#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FAULT_CODE_NONE = 0,
    FAULT_CODE_OVER_VOLTAGE,
    FAULT_CODE_UNDER_VOLTAGE,
    FAULT_CODE_DRV,
    FAULT_CODE_ABS_OVER_CURRENT,
    FAULT_CODE_OVER_TEMP_FET,
    FAULT_CODE_OVER_TEMP_MOTOR,
} mc_fault_code;

typedef enum {
    COMM_FW_VERSION = 0,
    COMM_JUMP_TO_BOOTLOADER,
    COMM_ERASE_NEW_APP,
    COMM_WRITE_NEW_APP_DATA,
    COMM_GET_VALUES,
    COMM_SET_DUTY,
    COMM_SET_CURRENT,
    COMM_SET_CURRENT_BRAKE,
    COMM_SET_RPM,
    COMM_SET_POS,
    COMM_SET_HANDBRAKE,
    COMM_SET_DETECT,
    COMM_SET_SERVO_POS,
    COMM_SET_MCCONF,
    COMM_GET_MCCONF,
    COMM_GET_MCCONF_DEFAULT,
    COMM_SET_APPCONF,
    COMM_GET_APPCONF,
    COMM_GET_APPCONF_DEFAULT,
    COMM_SAMPLE_PRINT,
    COMM_TERMINAL_CMD,
    COMM_PRINT,
    COMM_ROTOR_POSITION,
    COMM_EXPERIMENT_SAMPLE,
    COMM_DETECT_MOTOR_PARAM,
    COMM_DETECT_MOTOR_R_L,
    COMM_DETECT_MOTOR_FLUX_LINKAGE,
    COMM_DETECT_ENCODER,
    COMM_DETECT_HALL_FOC,
    COMM_REBOOT,
    COMM_ALIVE,
    COMM_GET_DECODED_PPM,
    COMM_GET_DECODED_ADC,
    COMM_GET_DECODED_CHUK,
    COMM_FORWARD_CAN,
    COMM_SET_CHUCK_DATA,
    COMM_CUSTOM_APP_DATA,
} COMM_PACKET_ID;

extern "C" {
int16_t buffer_get_int16(const uint8_t* buffer, int32_t* index);
int32_t buffer_get_int32(const uint8_t* buffer, int32_t* index);
float buffer_get_float16(const uint8_t* buffer, float scale, int32_t* index);
float buffer_get_float32(const uint8_t* buffer, float scale, int32_t* index);
void buffer_append_int32(uint8_t* buffer, int32_t number, int32_t* index);
void buffer_append_bool(uint8_t* buffer, bool value, int32_t* index);
unsigned short crc16(unsigned char* buf, unsigned int len);
}

#endif  // HELPERS_H_
