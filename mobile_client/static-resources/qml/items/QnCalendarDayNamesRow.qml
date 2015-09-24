import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: dayNames

    width: dp(48) * d.daysPerWeek
    height: row.height

    property bool mondayIsFirstDay: true

    QtObject {
        id: d

        readonly property var locale: Qt.locale()
        readonly property int daysPerWeek: 7
    }

    Row {
        id: row

        Repeater {
            model: ListModel {
                id: dayNamesModel
            }

            Item {
                width: dayNames.width / d.daysPerWeek
                height: dp(24)

                Text {
                    anchors.centerIn: parent
                    text: model.dayName
                    color: model.holiday ? QnTheme.calendarHolidayName : QnTheme.calendarDayName
                    font.pixelSize: sp(13)
                }
            }
        }
    }

    function updateDayNames() {
        dayNamesModel.clear()

        var s = mondayIsFirstDay ? 1 : 0
        for (var i = 0; i < d.daysPerWeek; ++i) {
            var day = (i + s) % d.daysPerWeek
            dayNamesModel.append({
                "dayName" : d.locale.dayName(day, Locale.ShortFormat),
                "holiday" : d.locale.weekDays.indexOf(day) == -1
            })
        }
    }

    onMondayIsFirstDayChanged: updateDayNames()
    Component.onCompleted: updateDayNames()
}

