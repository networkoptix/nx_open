import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: videoNavigation

    property var mediaPlayer

    implicitWidth: parent.width
    implicitHeight: content.height

    Image {
        width: parent.width
        height: sourceSize.height
        anchors.bottom: parent.bottom
        sourceSize.height: dp(56 * 2)
        source: "qrc:///images/timeline_gradient.png"
    }

    Column {
        id: content

        width: parent.width

        QnCircleProgressIndicator {
            width: dp(32)
            height: dp(32)
            anchors.horizontalCenter: parent.horizontalCenter
            lineWidth: dp(2)
            color: QnTheme.windowText

            opacity: mediaPlayer && mediaPlayer.loading ? 1.0 : 0.0

            Behavior on opacity {
                SequentialAnimation {
                    PauseAnimation { duration: 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }

        Text {
            id: liveLabel
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: sp(32)
            font.weight: Font.Normal
            color: QnTheme.windowText
            text: qsTr("LIVE")
            height: dp(64)
            verticalAlignment: Text.AlignVCenter
            opacity: mediaPlayer && mediaPlayer.loading ? 0.2 : 1.0

            Behavior on opacity {
                SequentialAnimation {
                    PauseAnimation { duration: 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }
    }
}
