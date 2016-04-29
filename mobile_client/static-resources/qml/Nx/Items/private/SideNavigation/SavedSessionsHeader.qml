import QtQuick 2.6
import Nx 1.0

Item
{
    implicitHeight: 56
    implicitWidth: parent ? parent.width : 200

    Text
    {
        id: text

        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        verticalAlignment: Text.AlignVCenter
        text: qsTr("Saved connections")
        font.pixelSize: 16
        font.weight: Font.DemiBold
        color: ColorTheme.brand_main
    }

    Rectangle
    {
        width: text.width
        height: 1
        x: text.x
        y: parent.height - 12
        color: ColorTheme.brand_main
    }
}
