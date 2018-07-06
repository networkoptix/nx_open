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
        id: contentItem

        readonly property real maximumHeight: 400
        readonly property real scaleFactor: height / maximumHeight

        anchors.centerIn: parent
        width: Math.min(availableWidth, height * 2)
        height: Math.min(availableHeight / 2, maximumHeight)

        Column
        {
            width: parent.width
            y: (parent.height - height) / 2

            Image
            {
                id: image

                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter

                width: parent.width
                height: implicitHeight * contentItem.scaleFactor
                fillMode: Image.PreserveAspectFit
            }

            Text
            {
                id: title

                width: parent.width
                height: contentItem.height - image.height

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 88 * contentItem.scaleFactor
                font.weight: Font.Light
                font.capitalization: Font.AllUppercase
                fontSizeMode: Text.Fit
                wrapMode: Text.WordWrap
                color: ColorTheme.red_main
            }
        }
    }
}
