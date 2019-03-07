import QtQuick 2.6
import Nx 1.0

Item
{
    id: control

    property alias label: labelControl
    property alias background: backgroundControl
    property int horizontalPadding: 16

    implicitHeight: labelControl.implicitHeight + 16
    implicitWidth: parent.width

    Rectangle
    {
        id: backgroundControl

        anchors.fill: parent
        color: "#61282B"
    }

    Text
    {
        id: labelControl

        x: control.horizontalPadding
        width: control.width - control.horizontalPadding * 2
        anchors.verticalCenter: parent.verticalCenter

        wrapMode: Text.WordWrap
        color: ColorTheme.text
        font.pixelSize: 13
    }
}
