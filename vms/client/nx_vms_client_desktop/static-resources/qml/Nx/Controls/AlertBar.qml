import QtQuick 2.6
import QtQuick.Controls 1.4
import Nx 1.0

Item
{
    id: control

    property alias label: labelControl
    property alias background: backgroundControl
    property int horizontalPadding: 16

    implicitHeight: 32
    implicitWidth: parent.width

    Rectangle
    {
        id: backgroundControl

        anchors.fill: parent
        color: "#61282B"
    }

    Label
    {
        id: labelControl

        color: ColorTheme.text
        font.pixelSize: 13
        width: control.width - control.horizontalPadding
        anchors.left: parent.left
        anchors.leftMargin: control.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
    }
}
