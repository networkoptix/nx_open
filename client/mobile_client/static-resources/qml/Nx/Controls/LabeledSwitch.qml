import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

import "private"

Control
{
    id: control

    property alias checked: indicator.checked
    property alias text: label.text

    signal clicked()

    padding: 9
    leftPadding: 16
    rightPadding: 16

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
            elide: Text.ElideRight
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
