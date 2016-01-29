import QtQuick 2.4

import "../main.js" as Main

Item {
    id: overlayLayer

    anchors.fill: parent

    property alias color: background.color
    property alias blocking: mouseArea.enabled

    signal closeRequested()

    visible: background.opacity > 0.0

    Rectangle {
        id: background
        anchors.fill: parent
        opacity: 0.0
        color: "transparent"
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: overlayLayer.closeRequested()
    }

    function show() {
        background.opacity = 1.0
    }

    function hide() {
        background.opacity = 0.0
    }

    focus: true

    Keys.onReleased: {
        if (!blocking)
            return

        if (Main.keyIsBack(event.key)) {
            if (visible) {
                overlayLayer.closeRequested()
                event.accepted = true
            }
        }
    }
}

