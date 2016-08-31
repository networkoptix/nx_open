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

    signal buttonClicked()

    padding: 0

    background: Rectangle { color: ColorTheme.windowBackground }

    contentItem: Item
    {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        Column
        {
            width: parent.width - 16
            x: 8
            y: (parent.height - height - (button.visible ? button.height : 0)) / 3
            spacing: 8

            Text
            {
                id: title
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 26
                font.weight: Font.Light
                lineHeight: 1.2
                color: ColorTheme.base15
                wrapMode: Text.WordWrap
                visible: text != ""
            }

            Text
            {
                id: description
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                lineHeight: 1.2
                color: ColorTheme.base15
                wrapMode: Text.WordWrap
                visible: text != ""
            }
        }

        Button
        {
            id: button
            width: parent.width
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            padding: 8
            onClicked: dummy.buttonClicked()
            visible: text != ""
        }
    }
}
