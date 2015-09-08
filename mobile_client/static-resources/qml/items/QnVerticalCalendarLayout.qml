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

    height: column.height

    Column {
        id: column

        width: parent.width

        Item {
            id: header

            height: dp(64)
            width: parent.width

            Row {
                spacing: dp(6)
                anchors.centerIn: parent

                Text {
                    id: monthLabel

                    color: QnTheme.calendarText
                    font.pixelSize: sp(18)
                    font.weight: Font.DemiBold
                }

                Text {
                    id: yearLabel

                    color: QnTheme.calendarText
                    font.pixelSize: sp(18)
                    font.weight: Font.DemiBold
                }
            }

            QnIconButton {
                id: previousMonthButton

                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                icon: "image://icon/arrow_left.png"
                explicitRadius: dp(24)
            }

            QnIconButton {
                id: nextMonthButton

                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                icon: "image://icon/arrow_right.png"
                explicitRadius: dp(24)
            }
        }

        QnCalendarDayNamesRow {
            id: dayNamesRow

            width: parent.width
            mondayIsFirstDay: mondayIsFirstDay
        }

        QnListView {
            id: monthsList

            width: parent.width
            height: 6 * dp(48)
            clip: true

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapOneItem
        }

        Item {
            id: footer

            width: parent.width
            height: dp(64)

            QnButton {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Close")
                icon: "image://icon/close.png"
                flat: true
                onClicked: calendarPanel.hide()
            }
        }
    }
}
