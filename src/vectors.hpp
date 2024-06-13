#include <cmath>

namespace rana {

struct Vec2 {
    float x, y;

    Vec2(float x, float y) : x(x), y(y) {}

    float dot(Vec2 rhs) { return x * rhs.x + y * rhs.y; }

    float length() { return std::sqrt(x*x + y*y); }

    Vec2 normalize() {
        auto len = length();
        if (len == 0.0f) {
            return *this;
        }
        return Vec2(x /= len, y /= len);
    }

    Vec2 operator+(Vec2 rhs) { return Vec2(x + rhs.x, y + rhs.y); }
    Vec2 operator-(Vec2 rhs) { return Vec2(x - rhs.x, y - rhs.y); }
    Vec2 operator*(Vec2 rhs) { return Vec2(x * rhs.x, y * rhs.y); }
    Vec2 operator/(Vec2 rhs) { return Vec2(x / rhs.x, y / rhs.y); }

    Vec2 operator+(float rhs) { return Vec2(x + rhs, y + rhs); }
    Vec2 operator-(float rhs) { return Vec2(x - rhs, y - rhs); }
    Vec2 operator*(float rhs) { return Vec2(x * rhs, y * rhs); }
    Vec2 operator/(float rhs) { return Vec2(x / rhs, y / rhs); }
};

}
