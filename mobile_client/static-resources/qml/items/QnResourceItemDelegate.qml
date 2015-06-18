import QtQuick 2.2

import "../main.js" as Main

Item {
    id: itemDelegate

    property string name
    property int type
    property string resourceId

    height: text.height

    Text {
        id: text
        text: name
        renderType: Text.NativeRendering
        color: QnTheme.windowText
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            Main.openVideo(resourceId)
        }
    }
}
