import QtQuick 2.6
import Nx 1.0

Column
{
    id: column

    property alias text: label.text

    width: parent ? parent.width : 0

    Image
    {
        source: lp("/images/cloud_big.png")
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Text
    {
        id: label
        text: applicationInfo.cloudName()
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: ColorTheme.windowText
        height: 32
        font.pixelSize: 20
        font.weight: Font.Light
        elide: Text.ElideRight
    }
}

