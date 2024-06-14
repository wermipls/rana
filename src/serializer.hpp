#pragma once

#include <vector>
#include <stdint.h>

namespace rana {

class Serializer;

class Serializable {
public:
    virtual void serialize(Serializer &s) = 0;
};

class Serializer {
    bool reading;
    std::vector<uint8_t> buffer;
    size_t position;

    void write(void *data, size_t bytes)
    {
        uint8_t *p = (uint8_t *)data;
        while (bytes--) {
            buffer.push_back(*p++);
        }
    }

    void read(void *data, size_t bytes)
    {
        uint8_t *p = (uint8_t *)data;
        while (bytes--) {
            *p++ = buffer[position++];
        }
    }

public:
    Serializer()
    {
        reading = false;
        buffer = std::vector<uint8_t>(0);
    }

    Serializer(std::vector<uint8_t> data)
    {
        buffer = data;
        position = 0;
    }

    std::vector<uint8_t> data()
    {
        return buffer;
    }

    void int8(int8_t *data)
    {
        if (reading) {
            read(data, 1);
        } else {
            write(data, 1);
        }
    }

    void int16(int16_t *data)
    {
        if (reading) {
            read(data, 2);
        } else {
            write(data, 2);
        }
    }

    void int32(int32_t *data)
    {
        if (reading) {
            read(data, 4);
        } else {
            write(data, 4);
        }
    }

    void int64(int64_t *data)
    {
        if (reading) {
            read(data, 8);
        } else {
            write(data, 8);
        }
    }

    void int8(uint8_t *data) { int8((int8_t *)data); }
    void int16(uint16_t *data) { int16((int16_t *)data); }
    void int32(uint32_t *data) { int32((int32_t *)data); }
    void int64(uint64_t *data) { int64((int64_t *)data); }

    // FIXME: not portable
    void float64(double *data)
    {
        if (reading) {
            read(data, sizeof(double));
        } else {
            write(data, sizeof(double));
        }
    }

    // FIXME: use appropriate type serializers
    template<typename T> void vector(std::vector<T> data) {
        if (reading) {
            uint32_t size;
            int32(&size);
            data = std::vector<T>(size / sizeof(T));
            read(data.data(), size);
        } else {
            uint32_t size = data.size() * sizeof(T);
            int32(&size);
            write(data.data(), size);
        }
    }
};

}