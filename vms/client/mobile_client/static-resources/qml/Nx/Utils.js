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
