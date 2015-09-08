import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: calendarMonth

    height: calendarGrid.height
    property alias year: calendarModel.year
    property alias month: calendarModel.month
    property date currentDate
    property alias mondayIsFirstDay: calendarModel.mondayIsFirstDay
    property alias chunkProvider: calendarModel.chunkProvider

    property alias dayHeight: calendarGrid.cellHeight

    property date _today: new Date()

    signal datePicked(date date)

    Grid {
        id: calendarGrid

        property real cellWidth: width / columns
        property real cellHeight: cellWidth

        width: parent.width - dp(32)
        x: dp(16)
        height: parent.width / columns * 6

        columns: 7

        Repeater {
            model: QnCalendarModel {
                id: calendarModel
                onMondayIsFirstDayChanged: dayNames.updateDayNames()
            }

            Item {
                id: calendarDay

                property bool current: currentDate.getMonth() + 1 == month &&
                                       currentDate.getFullYear() === model.date.getFullYear() &&
                                       currentDate.getMonth() === model.date.getMonth() &&
                                       currentDate.getDate() === model.date.getDate()

                width: calendarGrid.cellWidth
                height: calendarGrid.cellHeight

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: dp(3)
                    width: parent.width + dp(3)
                    height: dp(3)
                    radius: dp(1.5)
                    color: QnTheme.calendarArchiveIndicator
                    visible: model.hasArchive
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: dp(40)
                    height: dp(40)
                    radius: width / 2
                    visible: calendarDay.current
                    color: QnTheme.calendarSelectedBackground
                }

                Text {
                    anchors.centerIn: parent
                    text: model.display
                    color: calendarDay.current ? QnTheme.calendarSelectedText
                                               : model.date > _today ? QnTheme.calendarDisabledText : QnTheme.calendarText
                    font.pixelSize: sp(16)
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (model.date.getMonth() + 1 !== calendarModel.month ||
                            model.date > _today)
                        {
                            return
                        }

                        currentDate = model.date
                        calendarMonth.datePicked(model.date)
                    }
                }
            }
        }
    }
}

