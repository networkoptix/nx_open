import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0

import "private"

Control
{
    id: control

    property alias checked: indicator.checked
    property alias text: label.text

    signal clicked()

    padding: 9
    leftPadding: 8
    rightPadding: 8

    implicitWidth: label.implicitWidth + indicator.implicitWidth + spacing
        + leftPadding + rightPadding
    implicitHeight: 48

    background: MaterialEffect
    {
        clip: true
        rippleSize: 160
        mouseArea: mouseArea
    }

    contentItem: Row
    {
        id: row
        spacing: control.spacing

        Text
        {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - indicator.width - spacing
            color: ColorTheme.windowText
            font.pixelSize: 16
        }

        SwitchIndicator
        {
            id: indicator
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked:
        {
            control.checked = !control.checked
            control.clicked()
        }
    }
}
