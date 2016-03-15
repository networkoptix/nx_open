import QtQuick 2.0

import com.networkoptix.qml 1.0

import "../icons"
import "../controls"

Item {
    id: rootItem

    property bool paused: false
    property bool loading: false

    property bool speedEnabled: false
    readonly property alias dragging: d.dragging

    property alias gripTickVisible: tick.visible

    width: parent.width
    height: grip.height

    QtObject {
        id: d

        readonly property int minSpeed: -16
        readonly property int maxSpeed: 16
        property bool dragging: gripMouseArea.drag.active
    }

    readonly property real speed: {
        if (!d.dragging)
            return 1.0

        var center = (rootItem.width - grip.width) / 2
        var w = center - speedMarkers.padding
        var factor

        /* Positive and negative speeds have different scales.
           Coefficients are just evaluated to match grip position with the displayed speed.
         */
        // TODO: #dklychkov: Reimplement without magic numbers.
        if (grip.x < w) {
            factor = Math.pow((w - grip.x) / w, 4)
            return -1.0 + factor * (d.minSpeed + 1.0)
        } else if (grip.x > center + speedMarkers.padding) {
            factor = Math.pow((grip.x - w - speedMarkers.padding * 2) / w, 2.5)
            return 2.0 + factor * (d.maxSpeed - 2.0)
        } else {
            return 1.0
        }
    }

    Item {
        id: speedMarkers

        property real padding: grip.width / 6

        width: parent.width
        height: circle.height * 2 / 3
        y: circle.height / 6

        opacity: gripMouseArea.drag.active ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Rectangle {
            width: (parent.width - grip.width - speedMarkers.padding) / 2
            height: parent.height
            color: QnTheme.playPauseBackground
            radius: height / 2
            x: 0

            Text {
                text: "-16x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: speedMarkers.padding * 2
            }

            Text {
                text: "-4x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "-1x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: speedMarkers.padding * 2
            }
        }

        Rectangle {
            width: (parent.width - grip.width - speedMarkers.padding) / 2
            height: parent.height
            color: QnTheme.playPauseBackground
            radius: height / 2
            x: (parent.width + grip.width + speedMarkers.padding) / 2

            Text {
                text: "2x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: speedMarkers.padding * 2
            }

            Text {
                text: "8x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "16x"
                color: QnTheme.playPause
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: speedMarkers.padding * 2
            }
        }
    }

    Item {
        id: grip

        property real defaultX: (parent.width - width) / 2

        x: defaultX
        Binding {
            target: grip
            property: "x"
            value: grip.defaultX
            when: !gripMouseArea.pressed && !returnAnimation.running
        }

        height: dp(60)
        width: dp(60)

        Rectangle {
            id: circleBackground

            anchors.centerIn: circle
            anchors.alignWhenCentered: false
            width: dp(76)
            height: dp(76)
            color: QnTheme.playPauseBackground
            radius: width / 2
        }

        Rectangle {
            id: circle

            antialiasing: true

            anchors.centerIn: parent
            anchors.alignWhenCentered: false

            width: parent.width
            height: width
            radius: width / 2
            border.width: dp(2)
            border.color: QnTheme.playPause
            color: "transparent"
        }

        QnCircleProgressIndicator {
            id: preloader

            anchors.centerIn: circle
            width: circle.width - dp(6)
            height: circle.height - dp(6)
            color: QnTheme.playPause
            lineWidth: dp(2)
            opacity: rootItem.loading ? 0.5 : 0.0
            visible: opacity > 0
            Behavior on opacity {
                SequentialAnimation {
                    PauseAnimation { duration: preloader.visible ? 0 : 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }

        Rectangle {
            id: tick

            anchors.horizontalCenter: grip.horizontalCenter
            anchors.top: circle.bottom
            width: dp(2)
            height: dp(8)
            color: QnTheme.playPause
        }

        QnPlayPauseIcon {
            anchors.centerIn: circle
            width: dps(18)
            height: dps(18)
            scale: iconScale()
            pauseState: rootItem.paused
            color: QnTheme.playPause

            opacity: rootItem.loading ? 0.5 : 1.0
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        Rectangle {
            anchors.fill: circleBackground
            color: "black"
            radius: width / 2
            opacity: gripMouseArea.pressed ? 0.25 : 0.0
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            id: gripMouseArea

            anchors.fill: parent
            onClicked: rootItem.paused = !rootItem.paused

            drag.target: speedEnabled ? grip : undefined
            drag.axis: Drag.XAxis
            drag.minimumX: 0
            drag.maximumX: rootItem.width - grip.width

            onReleased: returnAnimation.start()
        }

        NumberAnimation on x {
            id: returnAnimation
            to: grip.defaultX
            duration: 500
            easing.type: Easing.OutCubic
            running: false
        }
    }
}
