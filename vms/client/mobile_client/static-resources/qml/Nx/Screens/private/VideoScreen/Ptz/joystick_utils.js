
var Type = {
    "Any": -1,
    "FreeWayPtz": 0,
    "EightWayPtz": 1,
    "FourWayPtz": 2,
    "TwoWayHorizontal": 3,
    "TwoWayVertical": 4}

/**
  * Eight-way ptz has 8 sectors with following layout:
  * 0 1 2
  * 7 - 3
  * 6 5 4
  *
  * All ptz with less ways count should map their sectors to listed above.
  * For example, four-way ptz layout is:
  *   1
  * 7 - 3
  *   4
  */

var Direction = {
    "Unknown": -1,
    "Top": 1,
    "Right": 3,
    "Bottom": 5,
    "Left": 7}

var ButtonRotations = [-1, -90, -1, 0, -1, 90, -1, 180]

var SectorData = {
    "TwoWayVertical": {
        "startAngle": 0,
        "step": Math.PI,
        "sectorsCount": 2,
        "buttons": [Direction.Bottom, Direction.Top]},

    "TwoWayHorizontal": {
        "startAngle": -Math.PI / 2,
        "step": Math.PI,
        "sectorsCount": 2,
        "buttons": [Direction.Right, Direction.Left]},

    "FourWayPtz": {
        "startAngle": -Math.PI / 4,
        "step": Math.PI / 2,
        "sectorsCount": 4,
        "buttons": [Direction.Right, Direction.Bottom, Direction.Left, Direction.Top]},

    "EightWayPtz": {
        "startAngle": -Math.PI * 7 / 8,
        "step": Math.PI / 4,
        "sectorsCount": 8,
        "buttons": [Direction.Right, Direction.Bottom, Direction.Left, Direction.Top]},

    "FreeWayPtz": {"buttons": [Direction.Right, Direction.Bottom, Direction.Left, Direction.Top]}}

function getSectorData(type)
{
    switch(type)
    {
        case Type.TwoWayHorizontal:
            return SectorData.TwoWayHorizontal
        case Type.TwoWayVertical:
            return SectorData.TwoWayVertical
        case Type.FourWayPtz:
            return SectorData.FourWayPtz
        case Type.EightWayPtz:
            return SectorData.EightWayPtz
        case Type.FreeWayPtz:
            return SectorData.FreeWayPtz
        default:
            return null //< No ptz support
    }
}

function getAngleWithOffset(startAngle, step, count)
{
    return startAngle + step * count
}

function directionToPosition(direction, maxLength, normalize)
{
    if (normalize)
        direction = direction.normalized()
    var vector = direction.times(maxLength)
    return vector.plus(d.centerPoint)
}

function getCosBetweenVectors(first, second)
{
    return first.normalized().dotProduct(second.normalized())
}

function fuzzyIsNull(value)
{
    var eps = 0.000001
    return value > -eps && value < eps
}

function getAngle(vector)
{
    var horizontalVector = Qt.vector2d(1, 0)
    var cosAlpha = getCosBetweenVectors(vector, horizontalVector)
    var sign = fuzzyIsNull(vector.y) ? 1 : Math.abs(vector.y) / vector.y
    var result = Math.acos(cosAlpha) * sign
    return result
}

function getDegrees(vector)
{
    return getAngle(vector) * 180 / Math.PI
}

function getRadialVector(radius, angle)
{
    return Qt.vector2d(radius * Math.cos(angle), radius * Math.sin(angle))
}

function pointInCircle(point, center, radius)
{
    return point.minus(center).length() <= radius
}

function to2PIRange(angle)
{
    var doublePi = Math.PI * 2
    while (angle < 0)
        angle += doublePi

    while (angle > doublePi)
        angle -= doublePi

    return angle
}

function angleInSector(point, center, startAngle, finishAngle)
{
    var vector = point.minus(center)
    var start = to2PIRange(startAngle)
    var finish = to2PIRange(finishAngle)
    var angle = to2PIRange(getAngle(vector))

    var result = start < finish
        ? angle >= start && angle <= finish
        : angle >= start || angle <= finish

    return result
}
