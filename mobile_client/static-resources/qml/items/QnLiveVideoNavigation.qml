import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: videoNavigation

    width: parent.width
    height: dp(56)

    Image {
        width: parent.width
        height: sourceSize.height
        anchors.bottom: parent.bottom
        sourceSize.height: dp(56 * 2)
        source: "qrc:///images/timeline_gradient.png"
    }

    Text {
        id: liveLabel
        anchors.centerIn: parent
        font.pixelSize: sp(32)
        font.weight: Font.Normal
        color: QnTheme.windowText
        text: qsTr("LIVE")
    }
}
