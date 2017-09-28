import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0

Rectangle
{
    property alias text: label.text
    property alias textColor: label.color

    color: ColorTheme.red_main
    radius: 2

    implicitHeight: 18
    implicitWidth: label.implicitWidth + 16

    Text
    {
        id: label
        anchors.centerIn: parent
        color: ColorTheme.base4
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
