import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item
{
    id: sideNavigationItem

    property alias active: background.active

    signal clicked()

    width: parent.width
    height: dp(48)

    Rectangle
    {
        id: focusRectangle

        visible: sideNavigationItem.activeFocus

        anchors.fill: parent
        color: QnTheme.sessionItemBackgroundActive

        Rectangle
        {
            height: parent.height
            width: dp(1)
            color: QnTheme.sessionItemActiveMark
        }
    }

    Rectangle
    {
        id: background

        property bool active

        visible: active

        anchors.fill: parent
        color: QnTheme.sessionItemBackgroundActive

        Rectangle
        {
            height: parent.height
            width: dp(3)
            color: QnTheme.sessionItemActiveMark
        }
    }

    QnMaterialSurface
    {
        onClicked: sideNavigationItem.clicked()
    }
}

