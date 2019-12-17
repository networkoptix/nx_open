// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vector2d.h"

#include <cmath>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

namespace {

static float fastInverseSqrt(float number)
{
    static constexpr float kThreeHalfs = 1.5F;
    long i;
    float x2, y;
    x2 = number * 0.5F;
    y = number;
    i = *(long*)& y;
    i = 0x5f3759df - (i >> 1);
    y = *(float*)& i;
    return y * (kThreeHalfs - (x2 * y * y));
}

} // namespace

Size::Size(float width, float height):
    width(width),
    height(height)
{
}

Vector2D::Vector2D(float x, float y):
    x(x),
    y(y)
{
}

 Vector2D& Vector2D::operator+=(const Vector2D& other)
{
    x += other.x;
    y += other.y;
    return *this;
}

 Vector2D& Vector2D::operator-=(const Vector2D& other)
{
    x -= other.x;
    y -= other.y;
    return *this;
}

 Vector2D& Vector2D::operator*=(float scalar)
 {
     x *= scalar;
     y *= scalar;
     return *this;
 }

 Vector2D& Vector2D::operator/=(float scalar)
 {
     x /= scalar;
     y /= scalar;
     return *this;
 }

Vector2D Vector2D::operator+(const Vector2D& other) const
{
    return Vector2D(x + other.x, y + other.y);
}

Vector2D Vector2D::operator-(const Vector2D& other) const
{
    return Vector2D(x - other.x, y - other.y);
}

Vector2D Vector2D::operator*(float scalar) const
{
   return Vector2D (x * scalar, y * scalar);
}

Vector2D Vector2D::operator/(float scalar) const
{
    return Vector2D(x / scalar, y / scalar);
}

float Vector2D::magnitude() const
{
    return std::sqrt(dotProduct(*this));
}

void Vector2D::normalize()
{
    operator*=(fastInverseSqrt(dotProduct(*this)));
}

Vector2D Vector2D::normalized() const
{
    Vector2D copy(x, y);
    copy.normalize();
    return copy;
}

float Vector2D::dotProduct(const Vector2D& other) const
{
    return x * other.x + y * other.y;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
