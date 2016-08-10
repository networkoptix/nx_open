import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Rectangle
{
    color: ColorTheme.base3

    Text
    {
        anchors.fill: parent
        text: qsTr("Turning on")
        color: ColorTheme.base13
        font.pixelSize: 24
        font.capitalization: Font.AllUppercase
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    ThreeDotBusyIndicator
    {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 3 / 4 - height / 2
    }
}

