import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0

Pane
{
    id: dummy

    property alias title: title.text
    property alias image: image.source

    padding: 0

    background: Rectangle { color: ColorTheme.windowBackground }

    contentItem: Item
    {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        Column
        {
            width: parent.width
            y: (parent.height - height) / 2

            Image
            {
                id: image
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text
            {
                id: title
                width: parent.width
                height: 160
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 88
                font.weight: Font.Light
                font.capitalization: Font.AllUppercase
                wrapMode: Text.WordWrap
                color: ColorTheme.red_main
            }
        }
    }
}
