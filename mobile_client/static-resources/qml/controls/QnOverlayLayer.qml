import QtQuick 2.4

Item {
    id: overlayLayer

    anchors.fill: parent

    property alias color: background.color
    property alias blocking: mouseArea.enabled

    signal dismiss()

    visible: background.opacity > 0.0

    Rectangle {
        id: background
        anchors.fill: parent
        opacity: 0.0
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: overlayLayer.dismiss()
    }

    function show() {
        background.opacity = 1.0
    }

    function hide() {
        dismiss()
        background.opacity = 0.0
    }

    focus: true

    Keys.onReleased: {
        if (!blocking)
            return

        if (event.key === Qt.Key_Back) {
            if (visible) {
                hide()
                event.accepted = true
            }
        }
    }
}

