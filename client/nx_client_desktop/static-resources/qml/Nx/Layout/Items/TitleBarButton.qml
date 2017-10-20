import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import nx.client.core 1.0

Button
{
    id: control

    property url icon
    property url checkedIcon: icon
    property color backgroundColor: ColorTheme.transparent(hoverColor, 0)
    property color hoverColor: ColorTheme.colors.brand_core
    property color pressedColor: ColorTheme.darker(ColorTheme.colors.brand_core, 3)
    property color checkedColor: ColorTheme.darker(ColorTheme.colors.brand_core, 1)

    leftPadding: 1
    rightPadding: 1
    topPadding: 1
    bottomPadding: 1

    implicitWidth: 26
    implicitHeight: 26

    height: parent ? Math.min(implicitHeight, parent.height) : implicitHeight
    width: height

    background: Rectangle
    {
        color:
        {
            if (control.pressed)
                return pressedColor

            if (mouseTracker.containsMouse)
                return hoverColor

            if (control.checked)
                return checkedColor

            return backgroundColor
        }
        Behavior on color { ColorAnimation { duration: 50 } }
    }

    label: Image
    {
        anchors.centerIn: control

        width: control.availableWidth
        height: control.availableHeight

        source: control.checked ? checkedIcon : icon
    }

    MouseTracker
    {
        id: mouseTracker
        item: control
        hoverEventsEnabled: true
    }
}
