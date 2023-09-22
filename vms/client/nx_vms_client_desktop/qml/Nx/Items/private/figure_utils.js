// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function relX(x, item)
{
    return x / item.width
}

function absX(x, item)
{
    return x * item.width
}

function relY(y, item)
{
    return y / item.height
}

function absY(y, item)
{
    return y * item.height
}

function toDegrees(alpha)
{
    return alpha / Math.PI * 180
}

function distance(x1, y1, x2, y2)
{
    return Math.sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))
}

function snapped(x, y, sx, sy, snapDistance)
{
    return distance(x, y, sx, sy) <= snapDistance ? Qt.point(sx, sy) : Qt.point(x, y)
}

function serializePoints(points)
{
    let result = []

    for (let i = 0; i < points.length; ++i)
    {
        const p = points[i]
        result.push([p.x, p.y])
    }

    return result
}

function deserializePoints(points)
{
    let result = []

    for (let i = 0; i < points.length; ++i)
    {
        const p = points[i]
        result.push(Qt.point(p[0], p[1]))
    }

    return result
}

function findSnapPoint(p, points, snapDistance)
{
    let minDistance = Infinity
    let pointIndex = -1

    for (let i = 0; i < points.length; ++i)
    {
        const dist = distance(p.x, p.y, points[i].x, points[i].y)
        if (dist <= snapDistance && dist < minDistance)
        {
            minDistance = dist
            pointIndex = i
        }
    }

    return pointIndex
}

function isEmptyObject(json)
{
    return !json || Object.keys(json).length === 0
}
