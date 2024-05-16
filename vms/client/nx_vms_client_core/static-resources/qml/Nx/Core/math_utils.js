// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

.pragma library

function fuzzyCompare(a, b)
{
    return Math.abs(Math.abs(a) - Math.abs(b)) < 1e-16
}

function bound(min, value, max)
{
    return Math.max(min, Math.min(max, value))
}

function toDegrees(rad)
{
    return rad / Math.PI * 180
}

function toRadians(deg)
{
    return deg / 180 * Math.PI
}

function log(base, value)
{
    return Math.log(value) / Math.log(base)
}

function pointAdd(p1, p2)
{
    return Qt.point(p1.x + p2.x, p1.y + p2.y)
}

function pointSub(p1, p2)
{
    return Qt.point(p1.x - p2.x, p1.y - p2.y)
}

function pointMul(p1, k)
{
    return Qt.point(p1.x * k, p1.y * k)
}

function pointDiv(p1, k)
{
    return Qt.point(p1.x / k, p1.y / k)
}

function scalarMul(v1, v2)
{
    return v1.x * v2.x + v1.y * v2.y
}

function vectorLength(v)
{
    return Math.sqrt(scalarMul(v, v))
}

function distance(p1, p2)
{
    return vectorLength(pointSub(p1, p2))
}

function normalized360(degrees) //< Angle, normalized in [0.0, 360.0) range.
{
    return ((degrees % 360.0) + 360.0) % 360
}

function normalized180(degrees) //< Angle, normalized in (-180.0, 180.0] range.
{
    return ((((degrees - 180.0) % 360.0) - 360.0) % 360) + 180.0
}

function visualNormalized180(degrees) //< Angle, normalized in [-180.0, 180.0] range.
{
    return (degrees < -180.0 || degrees > 180.0)
        ? normalized180(degrees)
        : degrees
}
