#pragma once

#include <stdint.h>
#include <stddef.h>

using uint8_t = ::uint8_t;
using uint16_t = ::uint16_t;
using uint32_t = ::uint32_t;
using ulong = unsigned long;

inline unsigned long millis() { return 0; }
inline void yield() {}

// Minimal Stream mock expected by VescUart
class Stream {
   public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual size_t write(const uint8_t* data, size_t len) {
        (void)data;
        (void)len;
        return 0;
    }
    // Minimal print helpers used by VescUart debug calls
    virtual void print(const char* s) { (void)s; }
    virtual void print(unsigned char v) { (void)v; }
    virtual void println(const char* s) { (void)s; }
    virtual void println() {}
    virtual ~Stream() {}
};

// Provide basic Serial-like flush/read/write if needed
extern Stream Serial2;