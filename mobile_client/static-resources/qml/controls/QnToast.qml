import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: toast

    property alias color: background.color
    property alias text: text.text
    property alias timeout: timeout.interval

    property alias mainButton: mainButton

    overlayLayer: "toastLayer"
    overlayColor: "transparent"

    width: parent.width
    height: text.lineCount > 1 ? dp(80) : dp(48)
    anchors.bottom: parent.bottom

    Rectangle {
        id: background
        anchors.fill: parent
        color: QnTheme.toastBackground
    }

    MouseArea {
        anchors.fill: parent
    }

    Item {
        id: content
        anchors.fill: parent
        anchors.leftMargin: dp(24)
        anchors.rightMargin: dp(16)

        Text {
            id: text
            anchors.verticalCenter: parent.verticalCenter
            color: QnTheme.toastText
            font.pixelSize: sp(14)
            width: parent.width - buttonBox.width - dp(8)
            wrapMode: Text.WordWrap
        }

        Row {
            id: buttonBox
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right

            QnButton {
                id: mainButton
                visible: text != "" || icon != ""
            }
        }
    }

    opacity: visible ? 1.0 : 0.0

    Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

    Timer {
        id: timeout
        interval: 0
        repeat: false
        running: false
        onTriggered: toast.close()
    }

    onOpened: {
        if (timeout.interval > 0)
            timeout.start()
    }
}

