import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0

AbstractButton
{
    id: control

    property string icon: ""
    property bool alwaysCompleteHighlightAnimation: true

    padding: 6

    implicitWidth: Math.max(48, background.implicitWidth, label.implicitWidth)
    implicitHeight: Math.max(48, background.implicitHeight, label.implicitHeight)

    label: Image
    {
        anchors.centerIn: parent
        source: control.icon
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: Item
    {
        implicitWidth: 36
        implicitHeight: 36

        MaterialEffect
        {
            anchors.fill: parent
            anchors.margins: control.padding
            rounded: true
            mouseArea: control
            rippleColor: "transparent"
            alwaysCompleteHighlightAnimation: control.alwaysCompleteHighlightAnimation
        }
    }
}
