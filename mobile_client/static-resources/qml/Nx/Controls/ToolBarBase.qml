import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

ToolBar
{
    id: toolBar

    default property alias data: contentItem.data

    property real statusBarHeight:
            (Qt.platform.os == "android" || Qt.platform.os == "ios") ? getStatusBarHeight : 0

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 56
    anchors.top: parent ? parent.top : undefined
    anchors.topMargin: statusBarHeight

    Item
    {
        id: contentItem
        anchors.fill: parent
        clip: true
    }

    background: defaultBackground

    Rectangle
    {
        id: defaultBackground

        color: ColorTheme.windowBackground

        Rectangle
        {
            width: parent.width
            height: statusBarHeight
            anchors.bottom: parent.top
            color: parent.color
        }

        Rectangle
        {
            width: parent.width
            height: 1
            anchors.top: parent.bottom
            color: ColorTheme.base4
        }
    }
}
