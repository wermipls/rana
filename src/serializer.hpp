#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <cassert>
#include <stdint.h>
#include "log.hpp"
#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
#endif

namespace rana {

class Serializer;

template <class T>
concept Serializable = requires(T a, Serializer &s) {
    a.serialize(s);
};

class Serializer {
protected:
    const char *error = nullptr;
    bool reading;

    Serializer(bool reading) : reading(reading) {}

    virtual void write_impl(const void *data, size_t bytes) = 0; // do not call directly - use write().
    virtual void read_impl(void *data, size_t bytes) = 0;  // do not call directly - use read().

    void write(const void *data, size_t bytes) {
        if (reading) [[unlikely]] {
            error = "not writable";
            return;
        }

        write_impl(data, bytes);
    }

    void read(void *data, size_t bytes) {
        if (!reading) [[unlikely]] {
            error = "not readable";
            return;
        }

        read_impl(data, bytes);

        // sometimes there's serialization code that depends on the read data
        // (e.g. resizing a vector to a value read from file).
        // this should make it safe even if user code makes poor assumptions,
        // or a serializer implementation is buggy and doesn't handle this.
        if (error) [[unlikely]] {
            memset(data, 0, bytes);
        }
    }

    void read_or_write(void *data, size_t bytes) {
        if (reading) {
            read(data, bytes);
        } else {
            write(data, bytes);
        }
    }

    void read_or_write_le16(void *p) {
        constexpr size_t bytes = 2;
        uint8_t tmp[bytes];
        auto data = (uint16_t *)p;

        if (reading) {
            read(tmp, bytes);
            *data = (uint16_t)tmp[0]
                  | (uint16_t)tmp[1] << 8;
        } else {
            tmp[0] = *data;
            tmp[1] = *data >> 8;
            write(tmp, bytes);
        }
    }

    void read_or_write_le32(void *p) {
        constexpr size_t bytes = 4;
        uint8_t tmp[bytes];
        auto data = (uint32_t *)p;

        if (reading) {
            read(tmp, bytes);
            *data = (uint32_t)tmp[0]
                  | (uint32_t)tmp[1] << 8
                  | (uint32_t)tmp[2] << 16
                  | (uint32_t)tmp[3] << 24;
        } else {
            tmp[0] = *data;
            tmp[1] = *data >> 8;
            tmp[2] = *data >> 16;
            tmp[3] = *data >> 24;
            write(tmp, bytes);
        }
    }

    void read_or_write_le64(void *p) {
        constexpr size_t bytes = 8;
        uint8_t tmp[bytes];
        auto data = (uint64_t *)p;

        if (reading) {
            read(tmp, bytes);
            *data = (uint64_t)tmp[0] 
                  | (uint64_t)tmp[1] << 8
                  | (uint64_t)tmp[2] << 16
                  | (uint64_t)tmp[3] << 24
                  | (uint64_t)tmp[4] << 32
                  | (uint64_t)tmp[5] << 40
                  | (uint64_t)tmp[6] << 48
                  | (uint64_t)tmp[7] << 56;
        } else {
            tmp[0] = *data;
            tmp[1] = *data >> 8;
            tmp[2] = *data >> 16;
            tmp[3] = *data >> 24;
            tmp[4] = *data >> 32;
            tmp[5] = *data >> 40;
            tmp[6] = *data >> 48;
            tmp[7] = *data >> 56;
            write(tmp, bytes);
        }
    }

    // those overloads just exist to make writing templates easier.
    // imho explicit function names are better in public api.
    void serialize_scalar(  int8_t &data) { int8(data); }
    void serialize_scalar( uint8_t &data) { int8(data); }
    void serialize_scalar( int16_t &data) { int16(data); }
    void serialize_scalar(uint16_t &data) { int16(data); }
    void serialize_scalar( int32_t &data) { int32(data); }
    void serialize_scalar(uint32_t &data) { int32(data); }
    void serialize_scalar( int64_t &data) { int64(data); }
    void serialize_scalar(uint64_t &data) { int64(data); }
    void serialize_scalar(   float &data) { float32(data); }
    void serialize_scalar(  double &data) { float64(data); }

public:
    bool is_reading() { return reading; }
    bool ok() { return error == nullptr; }
    const char *error_msg() { return error ? error : ""; }

    void int8( int8_t &data)   { read_or_write(&data, 1); }
    void int8(uint8_t &data)   { read_or_write(&data, 1); }
    void int16( int16_t &data) { read_or_write_le16(&data); }
    void int16(uint16_t &data) { read_or_write_le16(&data); }
    void int32( int32_t &data) { read_or_write_le32(&data); }
    void int32(uint32_t &data) { read_or_write_le32(&data); }
    void int64( int64_t &data) { read_or_write_le64(&data); }
    void int64(uint64_t &data) { read_or_write_le64(&data); }
    void float32(float &data)  { read_or_write_le32(&data); }
    void float64(double &data) { read_or_write_le64(&data); }

    void require_id_string(const std::string_view expected_id) {
        if (reading) {
            char actual_id[64]; // let's assume the user is not silly with the length.
            assert(expected_id.size() < sizeof(actual_id));

            read(actual_id, expected_id.size());
            if (memcmp(expected_id.data(), actual_id, expected_id.size()) != 0) {
                error = "format id mismatch";
            }
        } else {
            write(expected_id.data(), expected_id.size());
        }
    }

    void require_version(uint16_t expected_ver) {
        if (reading) {
            uint16_t actual_ver;
            int16(actual_ver);
            if (expected_ver != actual_ver) {
                error = "format version mismatch";
            }
        } else {
            int16(expected_ver);
        }
    }

    void bytes(std::vector<uint8_t> &data) {
        if (reading) {
            uint32_t size;
            int32(size);
            data.resize(size);
            read(data.data(), data.size());
        } else {
            if (data.size() > UINT32_MAX) {
                error = "vector.size() > UINT32_MAX";
                return;
            }
            uint32_t size = data.size();
            int32(size);
            write(data.data(), data.size());
        }
    }

    template<typename T> requires Serializable<T>
    void vector(std::vector<T> &data) {
        if (reading) {
            uint32_t size;
            int32(size);
            data.resize(size);
        } else {
            if (data.size() > UINT32_MAX) {
                error = "vector.size() > UINT32_MAX";
                return;
            }
            uint32_t size = data.size();
            int32(size);
        }

        for (auto &n : data) { n.serialize(*this); }
    }

    template<typename T>
    void vector(std::vector<T> &data) {
        if (reading) {
            uint32_t size;
            int32(size);
            data.resize(size);
        } else {
            if (data.size() > UINT32_MAX) {
                error = "vector.size() > UINT32_MAX";
                return;
            }
            uint32_t size = data.size();
            int32(size);
        }

        for (auto &n : data) { serialize_scalar(n); }
    }

    // a string of max 65535 characters
    void string16(std::string &str) {
        if (reading) {
            uint16_t size;
            int16(size);
            str.resize(size);
            read(str.data(), size);
        } else {
            if (str.size() > UINT16_MAX) {
                error = "string.size() > UINT16_MAX";
                return;
            }
            uint16_t size = str.size();
            int16(size);
            write(str.data(), size);
        }
    }

    // a string of max 255 characters
    void string8(std::string &str) {
        if (reading) {
            uint8_t size;
            int8(size);
            str.resize(size);
            read(str.data(), size);
        } else {
            if (str.size() > UINT8_MAX) {
                error = "string.size() > UINT8_MAX";
                return;
            }
            uint8_t size = str.size();
            int8(size);
            write(str.data(), size);
        }
    }
};

class SerializerVector : public Serializer {
protected:
    std::vector<uint8_t> &buffer;
    size_t position;

    SerializerVector(std::vector<uint8_t> &buffer, bool reading)
        : Serializer(reading)
        , buffer(buffer) 
    {
        if (reading) {
            position = 0;
        } else {
            position = buffer.size();
        }
    }

    virtual void write_impl(const void *data, size_t bytes) {
        buffer.resize(position + bytes);
        memcpy(buffer.data() + position, data, bytes);
        position += bytes;
    }

    virtual void read_impl(void *data, size_t bytes) {
        if (buffer.size() - position < bytes) {
            error = "not enough data";
            return;
        }

        memcpy(data, buffer.data() + position, bytes);
        position += bytes;
    }

public:
    static SerializerVector from(const std::vector<uint8_t> &buffer) {
        return SerializerVector(const_cast<std::vector<uint8_t> &>(buffer), true);
    }

    static SerializerVector into(std::vector<uint8_t> &buffer) {
        return SerializerVector(buffer, false);
    }
};

template <Serializable T>
bool serialize_into_vector(T &a, std::vector<uint8_t> &buffer)
{
    auto s = SerializerVector::into(buffer);
    a.serialize(s);

    auto ok = s.ok();
    if (!ok) {
        log::err("failed to serialize: %s", s.error_msg());
    }
    return ok;
}

template <Serializable T>
bool deserialize_from_vector(T &a, const std::vector<uint8_t> &buffer)
{
    auto s = SerializerVector::from(buffer);
    a.serialize(s);

    auto ok = s.ok();
    if (!ok) {
        log::err("failed to deserialize: %s", s.error_msg());
    }
    return ok;
}

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("Serializer - can serialize and deserialize little endian scalars") {
    uint16_t a = 0xcafe;
    uint32_t b = 0xdeadbeef;
    uint64_t c = 0xcafef00dabad1dea;
    float d = -6.2598534e18f; // 0xdeadbeef
    double e = -1.8520273620878089E53; // 0xcafef00dabad1dea

    std::vector<uint8_t> buf;
    auto ser = SerializerVector::into(buf);

    ser.int16(a);
    ser.int32(b);
    ser.int64(c);
    ser.float32(d);
    ser.float64(e);

    REQUIRE(ser.ok());
    REQUIRE(buf.size() == (2+4+8+4+8));

    auto p = buf.data();
    // a
    CHECK_EQ(p[1], 0xca);
    CHECK_EQ(p[0], 0xfe);
    p += 2;
    // b
    CHECK_EQ(p[3], 0xde);
    CHECK_EQ(p[2], 0xad);
    CHECK_EQ(p[1], 0xbe);
    CHECK_EQ(p[0], 0xef);
    p += 4;
    // c
    CHECK_EQ(p[7], 0xca);
    CHECK_EQ(p[6], 0xfe);
    CHECK_EQ(p[5], 0xf0);
    CHECK_EQ(p[4], 0x0d);
    CHECK_EQ(p[3], 0xab);
    CHECK_EQ(p[2], 0xad);
    CHECK_EQ(p[1], 0x1d);
    CHECK_EQ(p[0], 0xea);
    p += 8;
    // d
    CHECK_EQ(p[3], 0xde);
    CHECK_EQ(p[2], 0xad);
    CHECK_EQ(p[1], 0xbe);
    CHECK_EQ(p[0], 0xef);
    p += 4;
    // e
    CHECK_EQ(p[7], 0xca);
    CHECK_EQ(p[6], 0xfe);
    CHECK_EQ(p[5], 0xf0);
    CHECK_EQ(p[4], 0x0d);
    CHECK_EQ(p[3], 0xab);
    CHECK_EQ(p[2], 0xad);
    CHECK_EQ(p[1], 0x1d);
    CHECK_EQ(p[0], 0xea);

    auto deser = SerializerVector::from(buf);

    uint16_t a_roundtrip;
    uint32_t b_roundtrip;
    uint64_t c_roundtrip;
    float d_roundtrip;
    double e_roundtrip;

    deser.int16(a_roundtrip);
    deser.int32(b_roundtrip);
    deser.int64(c_roundtrip);
    deser.float32(d_roundtrip);
    deser.float64(e_roundtrip);

    REQUIRE(deser.ok());

    CHECK_EQ(a, a_roundtrip);
    CHECK_EQ(b, b_roundtrip);
    CHECK_EQ(c, c_roundtrip);
    CHECK_EQ(d, d_roundtrip);
    CHECK_EQ(e, e_roundtrip);
}

TEST_CASE("Serializer - fails gracefully on EOF") {
    std::vector<uint8_t> buf;
    
    SUBCASE("int32()") {
        auto deser = SerializerVector::from(buf);
        int32_t a = 0xbaadf00d;

        deser.int32(a);
        CHECK(!deser.ok());
        CHECK(std::string(deser.error_msg()) != "");
        CHECK_EQ(a, 0);
    }

    SUBCASE("bytes()") {
        auto deser = SerializerVector::from(buf);
        std::vector<uint8_t> v(67);

        deser.bytes(v);
        CHECK(!deser.ok());
        CHECK(std::string(deser.error_msg()) != "");
        CHECK_EQ(v.size(), 0);
    }

    SUBCASE("vector<Serializable>()") {
        struct abc {
            uint32_t a, b, c;

            void serialize(Serializer &s) {
                s.int32(a);
                s.int32(b);
                s.int32(c);
            }
        };

        std::vector<abc> v;
        auto deser = SerializerVector::from(buf);

        deser.vector(v);
        CHECK(!deser.ok());
        CHECK(std::string(deser.error_msg()) != "");
        CHECK_EQ(v.size(), 0);
    }
}

TEST_CASE("Serializer - fails gracefully on size limits") {
    std::vector<uint8_t> buf;

    SUBCASE("string8()") {
        auto ser = SerializerVector::into(buf);
        std::string str;
        str.resize(UINT8_MAX + 1);

        ser.string8(str);
        CHECK(!ser.ok());
        CHECK(std::string(ser.error_msg()) != "");
        CHECK_EQ(buf.size(), 0);
    }

    SUBCASE("string16()") {
        auto ser = SerializerVector::into(buf);
        std::string str;
        str.resize(UINT16_MAX + 1);

        ser.string16(str);
        CHECK(!ser.ok());
        CHECK(std::string(ser.error_msg()) != "");
        CHECK_EQ(buf.size(), 0);
    }
}

TEST_CASE("Serializer - writes and checks format identifier/version") {
    SUBCASE("require_version()") {
        std::vector<uint8_t> buf;
        {
            auto ser = SerializerVector::into(buf);
            ser.require_version(5);
    
            REQUIRE(buf.size() == 2);
            CHECK(ser.ok());
            CHECK(std::string(ser.error_msg()) == "");
            CHECK_EQ(buf[0], 5);
            CHECK_EQ(buf[1], 0);
        }

        {
            auto deser_valid = SerializerVector::from(buf);
            deser_valid.require_version(5);
            CHECK(deser_valid.ok());
            CHECK(std::string(deser_valid.error_msg()) == "");
        }

        {
            auto deser_invalid = SerializerVector::from(buf);
            deser_invalid.require_version(6);
            CHECK(!deser_invalid.ok());
            CHECK(std::string(deser_invalid.error_msg()) != "");
        }
    }

    SUBCASE("require_id_string()") {
        std::vector<uint8_t> buf;
        {
            auto ser = SerializerVector::into(buf);
            ser.require_id_string("pie");

            REQUIRE(buf.size() == 3);
            CHECK(ser.ok());
            CHECK(std::string(ser.error_msg()) == "");
            CHECK_EQ(buf[0], 'p');
            CHECK_EQ(buf[1], 'i');
            CHECK_EQ(buf[2], 'e');
        }

        {
            auto deser_valid = SerializerVector::from(buf);
            deser_valid.require_id_string("pie");
            CHECK(deser_valid.ok());
            CHECK(std::string(deser_valid.error_msg()) == "");
        }

        {
            auto deser_invalid = SerializerVector::from(buf);
            deser_invalid.require_id_string("cake");
            CHECK(!deser_invalid.ok());
            CHECK(std::string(deser_invalid.error_msg()) != "");
        }
    }
}

#endif // ifndef DOCTEST_CONFIG_DISABLE

} // namespace rana
