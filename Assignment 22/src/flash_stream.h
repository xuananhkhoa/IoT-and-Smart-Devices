#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <sfud.h>

#include "config.h"

class FlashStream : public Stream {
public:
    FlashStream() {
        _pos = 0;
        _flash_address = 0;
        _flash = sfud_get_device_table() + 0;

        populateBuffer();
    }

    virtual size_t write(uint8_t val) override {
        return 0;
    }

    virtual int available() override {
        int remaining = BUFFER_SIZE - ((_flash_address - HTTP_TCP_BUFFER_SIZE) + _pos);
        int bytes_available = min(HTTP_TCP_BUFFER_SIZE, remaining);

        return (bytes_available == 0) ? -1 : bytes_available;
    }

    virtual int read() override {
        int retVal = _buffer[_pos++];

        if (_pos == HTTP_TCP_BUFFER_SIZE) {
            populateBuffer();
        }

        return retVal;
    }

    virtual int peek() override {
        return _buffer[_pos];
    }

private:
    size_t _pos;
    size_t _flash_address;
    const sfud_flash* _flash;

    byte _buffer[HTTP_TCP_BUFFER_SIZE];

    void populateBuffer() {
        sfud_read(_flash, _flash_address, HTTP_TCP_BUFFER_SIZE, _buffer);
        _flash_address += HTTP_TCP_BUFFER_SIZE;
        _pos = 0;
    }
};
