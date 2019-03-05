import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import Nx 1.0

Control
{
    id: control

    property bool alwaysCompleteHighlightAnimation: true
    property color normalIconColor: "transparent"
    property color checkedIconColor: normalIconColor
    property color checkedBackgroundColor: ColorTheme.contrast1
    property int checkedPadding: 0
    property color backgroundColor: "transparent"
    property color disabledBackgroundColor: "transparent"
    property alias icon: iconLabel.icon
    property bool checkable: false
    property bool checked: false

    readonly property alias pressed: d.pressed

    signal pressedEvent()
    signal released()
    signal pressAndHold()
    signal canceled()
    signal clicked()

    function toggle()
    {
        if (checkable)
            checked = !checked
    }

    onClicked: toggle()

    onCheckableChanged:
    {
        if (!checkable)
            checked = false
    }

    padding: 6

    implicitWidth: Math.max(48, contentItem.implicitWidth)
    implicitHeight: Math.max(48, contentItem.implicitHeight)

    icon.color: checkable && checked
        ? checkedIconColor
        : normalIconColor

    contentItem: IconLabel
    {
        id: iconLabel

        anchors.centerIn: parent
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: MouseArea
    {
        id: background

        anchors.centerIn: parent
        width: control.width - control.padding * 2
        height: control.height - control.padding * 2

        function mouseOutsideButton(mouse)
        {
            var x = mouse.x
            var y = mouse.y
            return mouse.x < 0 || mouse.y < 0
                || mouse.x >= control.width || mouse.y >= control.height
        }

        onPressed:
        {
            d.pressed = true
            control.pressedEvent()
        }

        onPositionChanged: d.pressed = !mouseOutsideButton(mouse)

        onReleased:
        {
            d.pressedAndHeld = false
            d.pressed = false
            if (mouseOutsideButton(mouse))
            {
                control.canceled()
            }
            else
            {
                control.released()
                control.clicked()
            }
        }

        onPressAndHold:
        {
            control.pressAndHold()
            d.pressedAndHeld = true
        }

        onCanceled:
        {
            d.pressedAndHeld = false
            d.pressed = false
            control.canceled()
        }

        Rectangle
        {
            anchors.fill: parent
            radius: Math.min(width, height) / 2
            visible: !control.pressed
            color: control.enabled ? control.backgroundColor : control.disabledBackgroundColor
        }

        Rectangle
        {
            anchors.centerIn: parent
            width: parent.width - control.checkedPadding * 2
            height: parent.height - control.checkedPadding * 2
            radius: Math.min(width, height) / 2
            visible: control.checkable && control.checked
            color: control.checkedBackgroundColor
        }

        MaterialEffect
        {
            anchors.fill: parent
            rounded: true
            mouseArea: background
            rippleColor: "transparent"
            alwaysCompleteHighlightAnimation: control.alwaysCompleteHighlightAnimation
        }
    }

    QtObject
    {
        id: d

        property bool pressedAndHeld: false
        property bool pressed: false
    }
}
