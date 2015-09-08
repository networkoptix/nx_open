import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: layout

    readonly property alias monthLabel: monthLabel
    readonly property alias yearLabel: yearLabel
    readonly property alias previousMonthButton: previousMonthButton
    readonly property alias nextMonthButton: nextMonthButton
    readonly property alias monthsList: monthsList
    readonly property alias dayNamesRow: dayNamesRow

    width: header.width + column.width
    height: column.height + dp(32)

    Item {
        id: header

        height: parent.height
        width: dp(160)

        Column {
            width: parent.width
            y: dp(16)

            Text {
                id: yearLabel

                x: dp(16)
                color: QnTheme.calendarText
                font.pixelSize: sp(16)
                font.weight: Font.DemiBold
            }

            Text {
                id: monthLabel

                x: dp(16)
                color: QnTheme.calendarText
                font.pixelSize: sp(24)
                font.weight: Font.DemiBold
            }

            Item {
                width: 1
                height: dp(16)
            }

            Row {
                x: dp(4)

                QnIconButton {
                    id: previousMonthButton

                    icon: "image://icon/arrow_left.png"
                    explicitRadius: dp(24)
                }

                QnIconButton {
                    id: nextMonthButton

                    icon: "image://icon/arrow_right.png"
                    explicitRadius: dp(24)
                }
            }
        }

        QnButton {
            x: dp(4)
            anchors {
                bottom: parent.bottom
                bottomMargin: dp(8)
            }

            text: qsTr("Close")
            icon: "image://icon/close.png"
            flat: true
            onClicked: calendarPanel.hide()
        }

        Rectangle {
            anchors.right: parent.right
            height: parent.height - dp(32)
            width: dp(1)
            y: dp(16)
            color: "#2B383F"
        }
    }

    Column {
        id: column

        y: dp(16)
        anchors.left: header.right
        width: 7 * dp(48)
        spacing: dp(8)

        QnCalendarDayNamesRow {
            id: dayNamesRow

            width: parent.width
            mondayIsFirstDay: mondayIsFirstDay
        }

        QnListView {
            id: monthsList

            width: parent.width
            height: 6 * dp(40)
            clip: true

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapOneItem
        }
    }
}
