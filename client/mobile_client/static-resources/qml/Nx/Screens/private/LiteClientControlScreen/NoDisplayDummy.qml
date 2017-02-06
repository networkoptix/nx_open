import QtQuick 2.6
import Nx 1.0

Rectangle
{
    color: ColorTheme.base3

    Text
    {
        anchors.fill: parent
        anchors.margins: 16
        text: qsTr("No display connected")
        color: ColorTheme.base13
        font.pixelSize: 24
        font.capitalization: Font.AllUppercase
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}

