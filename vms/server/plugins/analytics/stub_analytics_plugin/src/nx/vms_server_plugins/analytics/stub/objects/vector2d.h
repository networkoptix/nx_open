// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

struct Size
{
    float width = 0;
    float height = 0;

    Size() = default;
    Size(float width, float height);
};

struct Vector2D
{
    float x = 0;
    float y = 0;

    Vector2D() = default;
    Vector2D(float x, float y);

    Vector2D operator+(const Vector2D& other) const;
    Vector2D operator-(const Vector2D& other) const;
    Vector2D operator*(float scalar) const;
    Vector2D operator/(float scalar) const;

    Vector2D& operator+=(const Vector2D& other);
    Vector2D& operator-=(const Vector2D& other);
    Vector2D& operator*=(float scalar);
    Vector2D& operator/=(float scalar);

    float magnitude() const;
    void normalize();
    Vector2D normalized() const;

    float dotProduct(const Vector2D& other) const;
};

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx