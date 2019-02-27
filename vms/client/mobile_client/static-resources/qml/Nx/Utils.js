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
    var firstVector = Qt.vector2d(first.x, first.y)
    var secondVector = Qt.vector2d(second.x, second.y)
    return firstVector.minus(secondVector).length() < maxDistance
}
