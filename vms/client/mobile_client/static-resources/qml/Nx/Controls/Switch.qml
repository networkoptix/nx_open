import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

import "private"

Control
{
    id: control

    property alias checked: indicator.checked

    signal clicked()

    padding: 6
    leftPadding: 8
    rightPadding: 8

    implicitWidth: indicator.implicitWidth + leftPadding + rightPadding
    implicitHeight: indicator.implicitHeight + topPadding + bottomPadding

    background: null

    contentItem: SwitchIndicator
    {
        id: indicator
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked:
        {
            control.checked = !control.checked
            control.clicked()
        }
    }
}
