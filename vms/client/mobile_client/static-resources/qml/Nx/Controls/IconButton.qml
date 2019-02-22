import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import Nx 1.0

AbstractButton
{
    id: control

    property bool alwaysCompleteHighlightAnimation: true
    property color normalIconColor: "transparent"
    property color checkedIconColor: normalIconColor
    property color checkedBackgroundColor: ColorTheme.contrast1
    property color backgroundColor: "transparent"
    property color disabledBackgroundColor: "transparent"

    padding: 6

    implicitWidth: Math.max(48, contentItem.implicitWidth)
    implicitHeight: Math.max(48, contentItem.implicitHeight)

    icon.color: checkable && checked
        ? checkedIconColor
        : normalIconColor

    onPressAndHold: { d.pressedAndHeld = true }
    onCanceled: { d.pressedAndHeld = false }
    onReleased:
    {
        if (!d.pressedAndHeld)
            return

        d.pressedAndHeld = false
        clicked()
    }

    contentItem: IconLabel
    {
        anchors.centerIn: parent
        icon: control.icon
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: Item
    {
        id: background

        width: control.width - control.padding * 2
        height: control.height - control.padding * 2

        anchors.centerIn: parent

        Rectangle
        {
            anchors.fill: parent
            radius: Math.min(width, height)
            visible: !control.pressed
            color: control.enabled ? control.backgroundColor : control.disabledBackgroundColor
        }

        Rectangle
        {
            anchors.fill: parent
            radius: Math.min(width, height)
            visible: control.checkable && control.checked
            color: control.checkedBackgroundColor
        }

        MaterialEffect
        {
            anchors.fill: parent
            rounded: true
            mouseArea: control
            rippleColor: "transparent"
            alwaysCompleteHighlightAnimation: control.alwaysCompleteHighlightAnimation
        }
    }

    QtObject
    {
        id: d

        property bool pressedAndHeld: false
    }
}
