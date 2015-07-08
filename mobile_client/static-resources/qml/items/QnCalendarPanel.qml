import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

import "../main.js" as Main

Item {
    id: calendarPanel

    parent: Main.findRootItem(calendarPanel)

    signal datePicked(date date)
    property alias date: calendar.date
    property alias chunkProvider: calendar.chunkProvider

    width: parent.width
    height: calendarContent.height
    z: 100.0

    visible: opacity > 0.0
    opacity: 0.0

    readonly property real _yAnimationShift: dp(56)

    Rectangle {
        parent: calendarPanel.parent
        anchors.fill: parent

        opacity: calendarPanel.opacity
        color: "#66171c1f"
        z: 10.0
        visible: opacity > 0.0

        MouseArea {
            anchors.fill: parent
            onClicked: calendarPanel.hide()
        }
    }

    Rectangle {
        anchors.fill: calendarContent
        color: QnTheme.calendarBackground

        QnSideShadow {
            anchors.fill: parent
            position: Qt.TopEdge
            z: 10
        }

        MouseArea {
            anchors.fill: parent
            /* To block mouse events under calendar */
        }
    }

    Column {
        id: calendarContent
        width: parent.width

        QnChunkedCalendar {
            id: calendar
            width: parent.width
            onDatePicked: calendarPanel.datePicked(date)
        }

        QnButton {
            anchors.left: parent.left
            text: qsTr("Close")
            flat: true
            onClicked: calendarPanel.hide()
        }
    }

    SequentialAnimation {
        id: showAnimation

        ParallelAnimation {
            NumberAnimation {
                target: calendarPanel
                property: "opacity"
                duration: 150
                easing.type: Easing.OutCubic
                to: 1.0
            }

            NumberAnimation {
                target: calendarContent
                property: "y"
                duration: 150
                easing.type: Easing.OutCubic
                from: calendarPanel.parent.height - calendarPanel.height + _yAnimationShift
                to: calendarPanel.parent.height - calendarPanel.height
            }
        }
    }

    SequentialAnimation {
        id: hideAnimation

        ParallelAnimation {
            NumberAnimation {
                target: calendarPanel
                property: "opacity"
                duration: 150
                easing.type: Easing.OutCubic
                to: 0.0
            }

            NumberAnimation {
                target: calendarContent
                property: "y"
                duration: 150
                easing.type: Easing.OutCubic
                to: calendarPanel.parent.height - calendarPanel.height + _yAnimationShift
            }
        }
    }

    function show() {
        hideAnimation.stop()
        showAnimation.start()
    }

    function hide() {
        showAnimation.stop()
        hideAnimation.start()
    }

    focus: true

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (visible) {
                hide()
                event.accepted = true
            }
        }
    }
}
