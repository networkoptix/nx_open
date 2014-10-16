import QtQuick 2.2

import "../main.js" as Main

Item {
    id: itemDelegate

    property string name
    property int type
    property string resourceId

    property var __syspal: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    height: text.height

    Text {
        id: text
        text: name
        renderType: Text.NativeRendering
        color: __syspal.windowText
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            Main.openVideo(resourceId)
        }
    }
}
