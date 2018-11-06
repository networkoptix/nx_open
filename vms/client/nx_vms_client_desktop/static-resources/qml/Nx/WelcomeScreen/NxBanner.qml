import QtQuick 2.6
import Nx 1.0

Item
{
    id: control

    property alias textControl: captionControl
    property alias background: backgroundControl
    property int horizontalPadding: 12

    implicitHeight: 32
    implicitWidth: captionControl.implicitWidth + horizontalPadding * 2

    Rectangle
    {
        id: backgroundControl

        radius: 2
        anchors.fill: parent
        color: ColorTheme.transparent(ColorTheme.colors.red_main, 0.2)
    }

    NxLabel
    {
        id: captionControl

        color: "#cf8989"
        width: control.width - control.horizontalPadding
        anchors.left: parent.left
        anchors.leftMargin: control.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
    }

}
