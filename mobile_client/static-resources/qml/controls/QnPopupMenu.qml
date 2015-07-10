import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: menu

    overlayLayer: "popupLayer"
    overlayColor: "transparent"

    readonly property real padding: dp(4)

    Rectangle {
        anchors.fill: parent
        anchors.margins: padding
        color: QnTheme.popupMenuBackground
        radius: dp(2)
    }
}

