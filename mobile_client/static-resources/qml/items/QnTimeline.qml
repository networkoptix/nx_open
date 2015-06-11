import QtQuick 2.0

import com.networkoptix.qml 1.0

Item {
    id: root

    property alias startDate: timeline.windowStartDate
    property alias endDate: timeline.windowEndDate
    property alias positionDate: timeline.positionDate
    property alias textColor: timeline.textColor
    property alias chunkColor: timeline.chunkColor
    property alias chunkBarHeight: timeline.chunkBarHeight
    property alias stickToEnd: timeline.stickToEnd
    property alias font: timeline.font
    property alias chunkProvider: timeline.chunkProvider
    property alias startBoundDate: timeline.startBoundDate
    property alias autoPlay: timeline.autoPlay

    signal dragFinished()
    signal moveFinished()

    function zoomIn() {
        timeline.zoomIn()
    }

    function zoomOut() {
        timeline.zoomOut()
    }

    QnTimelineView {
        id: timeline
        anchors.fill: parent

        onMoveFinished: root.moveFinished()
    }

    PinchArea {
        anchors.fill: parent
        pinch.minimumRotation: 0
        pinch.maximumRotation: 0
        pinch.dragAxis: Qt.XAxis

        onPinchStarted: timeline.startPinch(pinch.center.x, pinch.scale)
        onPinchUpdated: timeline.updatePinch(pinch.center.x, pinch.scale)
        onPinchFinished: {
            timeline.finishPinch(pinch.center.x, pinch.scale)
            root.dragFinished()
        }

        MouseArea {
            property int pressX: -1

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton

            onPressed: pressX = mouseX
            onPositionChanged: {
                if (pressX != -1 && Math.abs(pressX - mouseX) > drag.threshold) {
                    timeline.startDrag(pressX)
                    pressX = -1
                }
                timeline.updateDrag(mouse.x)
            }
            onReleased: {
                pressX = -1
                timeline.finishDrag(mouse.x)
                root.dragFinished()
            }
            onDoubleClicked: timeline.zoomIn(mouse.x)
        }
    }
}

