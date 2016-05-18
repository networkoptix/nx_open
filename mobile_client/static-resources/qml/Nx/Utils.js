function isMobile()
{
    return Qt.platform.os == "android" || Qt.platform.os == "ios"
}

function keyIsBack(key)
{
    return key == Qt.Key_Back || (liteMode && key == Qt.Key_Escape)
}
