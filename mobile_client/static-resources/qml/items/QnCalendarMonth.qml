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

    property date _today: new Date()

    signal datePicked(date date)

    Grid {
        id: calendarGrid

        property real cellWidth: width / columns

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
                height: width

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: dp(8)
                    width: parent.width
                    height: dp(3)
                    color: QnTheme.calendarArchiveIndicator
                    visible: model.hasArchive
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: dp(8)
                    visible: calendarDay.current
                    radius: width / 2
                    color: QnTheme.calendarSelectedBackground
                }

                Text {
                    anchors.centerIn: parent
                    text: model.display
                    color: calendarDay.current ? QnTheme.calendarSelectedText
                                               : model.date > _today ? QnTheme.calendarDisabledText : QnTheme.calendarText
                    font.pixelSize: sp(14)
                    font.weight: Font.Light
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

