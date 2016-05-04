import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

Button
{
    id: control

    property string icon: ""

    label: Image
    {
        anchors.centerIn: parent
        source: control.icon
        opacity: control.enabled ? 1.0 : 0.3
    }

    background: MaterialEffect
    {
        implicitWidth: 48
        implicitHeight: 48
        rounded: true
        mouseArea: control
        rippleColor: "transparent"
    }
}
