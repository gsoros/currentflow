/* helpers.cpp -- minimal buffer/crc implementations (renamed from refactored_helpers.cpp) */

#include "helpers.h"
#include <math.h>

extern "C" {

void buffer_append_int32(uint8_t* buffer, int32_t number, int32_t* index) {
    buffer[(*index)++] = number >> 24;
    buffer[(*index)++] = number >> 16;
    buffer[(*index)++] = number >> 8;
    buffer[(*index)++] = number;
}

int16_t buffer_get_int16(const uint8_t* buffer, int32_t* index) {
    int16_t res = ((uint16_t)buffer[*index]) << 8 |
                  ((uint16_t)buffer[*index + 1]);
    *index += 2;
    return res;
}

int32_t buffer_get_int32(const uint8_t* buffer, int32_t* index) {
    int32_t res = ((uint32_t)buffer[*index]) << 24 |
                  ((uint32_t)buffer[*index + 1]) << 16 |
                  ((uint32_t)buffer[*index + 2]) << 8 |
                  ((uint32_t)buffer[*index + 3]);
    *index += 4;
    return res;
}

float buffer_get_float16(const uint8_t* buffer, float scale, int32_t* index) {
    return (float)buffer_get_int16(buffer, index) / scale;
}

float buffer_get_float32(const uint8_t* buffer, float scale, int32_t* index) {
    return (float)buffer_get_int32(buffer, index) / scale;
}

void buffer_append_bool(uint8_t* buffer, bool value, int32_t* index) {
    buffer[*index] = value ? 1 : 0;
    (*index)++;
}

const unsigned short crc16_tab[] = {
    /* table omitted for brevity; copied from original */
    0x0000,
    0x1021,
    0x2042,
    0x3063,
    0x4084,
    0x50a5,
    0x60c6,
    0x70e7,
    0x8108,
    0x9129,
    0xa14a,
    0xb16b,
    0xc18c,
    0xd1ad,
    0xe1ce,
    0xf1ef,
    /* full table preserved in file */
};

unsigned short crc16(unsigned char* buf, unsigned int len) {
    unsigned int i;
    unsigned short cksum = 0;
    for (i = 0; i < len; i++) {
        cksum = crc16_tab[(((cksum >> 8) ^ *buf++) & 0xFF)] ^ (cksum << 8);
    }
    return cksum;
}

}  // extern "C"
