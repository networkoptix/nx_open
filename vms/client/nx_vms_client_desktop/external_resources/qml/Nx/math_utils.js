.pragma library

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
