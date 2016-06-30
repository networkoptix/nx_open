import QtQuick 2.0
import Qt.labs.controls 1.0
import Nx 1.0

Popup
{
    id: control

    property bool deleteOnClose: false

    implicitWidth: Math.min(328, parent.width - 32)
    implicitHeight: Math.min(parent.height - 56 * 2, contentItem.implicitHeight)
    x: (parent.width - implicitWidth) / 2
    y: (parent.height - implicitHeight) / 2
    padding: 0
    modal: true

    background: Rectangle { color: ColorTheme.contrast3 }

    readonly property int _animationDuration: 200

    signal closed()
    signal opened()

    enter: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: _animationDuration
            easing.type: Easing.OutCubic
            to: 1.0
        }
    }

    exit: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: _animationDuration
            easing.type: Easing.OutCubic
            to: 0.0
        }
    }

    onVisibleChanged:
    {
        if (!visible)
        {
            control.closed()

            if (deleteOnClose)
                destroy()
        }
        else
        {
            control.opened()
        }
    }

    onOpened: contentItem.forceActiveFocus()
}
