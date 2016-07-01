import QtQuick 2.6
import Nx 1.0

Rectangle
{
    property alias text: label.text

    color: ColorTheme.red_main
    radius: 2
    implicitHeight: 18
    implicitWidth: label.implicitWidth

    Text
    {
        id: label
        anchors.fill: parent
        leftPadding: 8
        rightPadding: 8
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: ColorTheme.base4
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
