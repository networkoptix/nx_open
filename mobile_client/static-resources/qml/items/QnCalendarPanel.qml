import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

import "../main.js" as Main

QnPopup {
    id: calendarPanel

    parent: widgetLayer

    signal datePicked(date date)
    property alias date: calendar.date
    property alias chunkProvider: calendar.chunkProvider

    width: parent.width
    height: calendar.height
    anchors.bottom: parent.bottom

    opacity: 0.0
    hideLayerAfterAnimation: false

    Rectangle {
        anchors.fill: calendar
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

    QnChunkedCalendar {
        id: calendar
        width: parent.width
        onDatePicked: calendarPanel.datePicked(date)
    }

    showAnimation: SequentialAnimation {
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
                target: calendarPanel
                property: "anchors.bottomMargin"
                duration: 150
                easing.type: Easing.OutCubic
                from: -dp(56)
                to: 0
            }
        }
    }

    hideAnimation: SequentialAnimation {
        id: hideAnimation

        ParallelAnimation {
            ScriptAction {
                script: calendarPanel.parent.hide()
            }

            NumberAnimation {
                target: calendarPanel
                property: "opacity"
                duration: 150
                easing.type: Easing.OutCubic
                to: 0.0
            }

            NumberAnimation {
                target: calendarPanel
                property: "anchors.bottomMargin"
                duration: 150
                easing.type: Easing.OutCubic
                to: -dp(56)
            }
        }
    }
}
