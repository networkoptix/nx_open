import QtQuick 2.6
import Qt.labs.controls 1.0
import Nx 1.0
import Nx.Controls 1.0

Pane
{
    id: dummy

    property alias title: title.text
    property alias description: description.text
    property alias buttonText: button.text
    property alias image: image.source
    property bool compact: false
    property bool centered: false

    signal buttonClicked()

    padding: 0

    background: Rectangle { color: ColorTheme.windowBackground }

    contentItem: Item
    {
        Column
        {
            width: parent.width
            y: (parent.height - height) / (centered ? 2 : 4)

            Image
            {
                id: image
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !compact && status == Image.Ready
            }

            Text
            {
                id: title
                width: parent.width
                topPadding: 8
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 22
                lineHeight: 1.25
                color: ColorTheme.contrast16
                wrapMode: Text.WordWrap
                visible: text != ""
            }

            Text
            {
                id: description
                topPadding: 8
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                lineHeight: 1.25
                color: ColorTheme.base16
                wrapMode: Text.WordWrap
                visible: text != ""
            }
        }

        Button
        {
            id: button
            anchors.bottom: parent.bottom
            anchors.bottomMargin: compact ? 32 : 64
            anchors.horizontalCenter: parent.horizontalCenter
            padding: 16
            onClicked: dummy.buttonClicked()
            visible: text != ""
        }
    }
}
