#include "vector2.h"

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

Vector2::Vector2(float x, float y):
    x(x),
    y(y)
{
}

 Vector2& Vector2::operator+=(const Vector2& rhs)
{
    x += rhs.x;
    y += rhs.y;
    return *this;
}

 Vector2& Vector2::operator-=(const Vector2& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    return *this;
}

 Vector2& Vector2::operator*=(float scalar)
 {
     x *= scalar;
     y *= scalar;
     return *this;
 }

 Vector2& Vector2::operator/=(float scalar)
 {
     x /= scalar;
     y /= scalar;
     return *this;
 }

Vector2 Vector2::operator+(const Vector2& rhs) const
{
    return Vector2(x + rhs.x, y + rhs.y);
}

Vector2 Vector2::operator-(const Vector2& rhs) const
{
    return Vector2(x - rhs.x, y - rhs.y);
}

Vector2 Vector2::operator*(float scalar) const
{
   return Vector2 (x * scalar, y * scalar);
}

Vector2 Vector2::operator/(float scalar) const
{
    return Vector2(x / scalar, y / scalar);
}

float Vector2::magnitude() const
{
    return std::sqrt(dot(*this));
}

void Vector2::normalize()
{
    operator*=(fastInverseSqrt(dot(*this)));
}

Vector2 Vector2::normalized() const
{
    Vector2 copy(x, y);
    copy.normalize();
    return copy;
}

float Vector2::dot(const Vector2& rhs) const
{
    return x * rhs.x + y * rhs.y;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx