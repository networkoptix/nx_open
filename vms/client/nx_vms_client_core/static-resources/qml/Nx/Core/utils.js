// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

.pragma library

function isEmptyValue(value)
{
    return value === undefined || value === null
}

function toBoolean(value)
{
    return value && (typeof value !== "string" || /true/i.test(value))
}

function isObject(object)
{
    return object !== null && typeof object === 'object';
}

function isFunction(object)
{
    return object !== null && typeof object === 'function'
}

function equalValues(left, right)
{
    if (typeof left !== typeof right)
        return false

    if (isFunction(left))
        return left.toString() === right.toString();

    if (!isObject(left))
        return left === right

    const leftKeys = Object.keys(left)
    const rightKeys = Object.keys(right ?? {})
    if (leftKeys.length !== rightKeys.length)
    {
        return false
    }

    for (const key of leftKeys)
    {
        if (!equalValues(left[key], right[key]))
            return false
    }
    return true
}

function isRotated90(angle)
{
    return angle % 90 == 0 && angle % 180 != 0
}

function getValue(value, defaultValue)
{
    return value !== undefined ? value : defaultValue
}

function toArray(list)
{
    if (Array.isArray(list))
        return list

    var result = []
    if (!list)
        return result

    for (var i = 0; i < list.length; ++i)
        result.push(list[i])

    return result
}

function isMobile()
{
    return Qt.platform.os == "android" || Qt.platform.os == "ios"
}

function keyIsBack(key)
{
    return key == Qt.Key_Back || key == Qt.Key_Escape
}

function isRotated90(angle)
{
    return angle % 90 == 0 && angle % 180 != 0
}

function nearPositions(first, second, maxDistance)
{
    if (!first || !second || !maxDistance)
        return false

    var firstVector = Qt.vector2d(first.x, first.y)
    var secondVector = Qt.vector2d(second.x, second.y)
    return firstVector.minus(secondVector).length() < maxDistance
}

function executeDelayed(callback, delay, parent)
{
    if (!callback || delay < 0 || !parent)
    {
        console.error("Wrong parameters for executeDelayed function")
        return
    }

    var TimerCreator =
        function()
        {
            return Qt.createQmlObject("import QtQuick 2.0; Timer {}", parent)
        }

    var timer = new TimerCreator()
    timer.interval = delay;
    timer.repeat = false;
    timer.triggered.connect(
        function()
        {
            callback()
            timer.destroy()
        })
    timer.start()
}

function executeLater(callback, parent)
{
    executeDelayed(callback, 0, parent)
}
