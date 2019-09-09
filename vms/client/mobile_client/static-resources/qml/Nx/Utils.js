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
