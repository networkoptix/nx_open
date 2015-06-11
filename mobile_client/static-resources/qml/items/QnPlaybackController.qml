import QtQuick 2.0

import "../icons"

Item {
    id: rootItem

    property bool paused: false

    property color color: "black"
    property color markersBackground: "grey"
    property color highlightColor: "#60000000"
    property real tickSize: 10
    property real lineWidth: 2
    property bool speedEnabled: true
    readonly property alias dragging: rootItem.__dragging

    readonly property int __minSpeed: -16
    readonly property int __maxSpeed: 16
    property bool __dragging: gripMouseArea.drag.active

    readonly property real speed: {
        if (!__dragging)
            return 1.0

        var center = (rootItem.width - grip.width) / 2
        var w = center - speedMarkers.padding
        var factor

        if (grip.x < w) {
            factor = Math.pow((w - grip.x) / w, 4)
            return -1.0 + factor * (__minSpeed + 1.0)
        } else if (grip.x > center + speedMarkers.padding) {
            factor = Math.pow((grip.x - w - speedMarkers.padding * 2) / w, 2.5)
            return 2.0 + factor * (__maxSpeed - 2.0)
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
            color: markersBackground
            radius: height / 2
            x: 0

            Text {
                text: "-16x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: speedMarkers.padding * 2
            }

            Text {
                text: "-4x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "-1x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: speedMarkers.padding * 2
            }
        }

        Rectangle {
            width: (parent.width - grip.width - speedMarkers.padding) / 2
            height: parent.height
            color: markersBackground
            radius: height / 2
            x: (parent.width + grip.width + speedMarkers.padding) / 2

            Text {
                text: "2x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: speedMarkers.padding * 2
            }

            Text {
                text: "8x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "16x"
                color: rootItem.color
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: speedMarkers.padding * 2
            }
        }
    }

    Item {
        id: grip

        property real defaultX: (parent.width - width) / 2
        property color idleColor: Qt.rgba(rootItem.color.r, rootItem.color.g, rootItem.color.b, 0)

        x: defaultX

        height: parent.height - tickSize
        width: height

        Rectangle {
            anchors.centerIn: circle
            width: circle.height + 2 * tickSize
            height: width
            color: "#48000000"
            radius: width / 2
        }

        Rectangle {
            id: circle

            antialiasing: true

            width: parent.width

            height: width
            radius: width / 2
            border.width: lineWidth
            border.color: rootItem.color
            color: gripMouseArea.drag.active ? highlightColor : grip.idleColor
        }

        Rectangle {
            id: tick

            anchors.horizontalCenter: grip.horizontalCenter
            anchors.top: circle.bottom
            width: lineWidth
            height: tickSize
            color: rootItem.color
        }

        QnPlayPauseIcon {
            anchors.centerIn: circle
            width: circle.width * 0.4
            height: circle.height * 0.4
            pauseState: rootItem.paused
            color: rootItem.color
        }

        MouseArea {
            id: gripMouseArea

            anchors.fill: parent
            onClicked: rootItem.paused = !rootItem.paused

            drag.target: speedEnabled ? grip : undefined
            drag.axis: Drag.XAxis
            drag.minimumX: 0
            drag.maximumX: rootItem.width - grip.width

            onReleased: grip.x = grip.defaultX
        }

        Behavior on x { NumberAnimation { duration: 200 } }
    }
}
