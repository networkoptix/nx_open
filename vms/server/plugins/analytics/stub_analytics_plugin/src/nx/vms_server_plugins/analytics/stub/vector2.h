#pragma once

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

struct Size
{
    float width = 0;
    float height = 0;
};

struct Vector2
{
    float x = 0;
    float y = 0;

    Vector2() = default;
    Vector2(float x, float y);

    Vector2 operator+(const Vector2& rhs) const;
    Vector2 operator-(const Vector2& rhs) const;
    Vector2 operator*(float scalar) const;
    Vector2 operator/(float scalar) const;

    Vector2& operator+=(const Vector2& rhs);
    Vector2& operator-=(const Vector2& rhs);
    Vector2& operator*=(float scalar);
    Vector2& operator/=(float scalar);

    float magnitude() const;
    void normalize();
    Vector2 normalized() const;

    float dot(const Vector2& rhs) const;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx