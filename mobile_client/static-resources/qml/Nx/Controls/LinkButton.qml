import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

Button
{
    id: control

    property color color: ColorTheme.blue_d2

    implicitHeight: 32
    implicitWidth: label ? label.implicitWidth : 0

    background: null
    label: Text
    {
        text: control.text
        width: control.width
        height: control.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: control.color
        font.pixelSize: 15
        font.underline: true
    }

    MouseArea
    {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
