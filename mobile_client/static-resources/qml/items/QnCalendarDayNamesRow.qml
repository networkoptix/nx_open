import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: dayNames

    width: dp(48) * 7
    height: row.height

    property bool mondayIsFirstDay: true

    QtObject {
        id: d

        property var locale: Qt.locale()
    }

    Row {
        id: row

        Repeater {
            model: ListModel {
                id: dayNamesModel
            }

            Item {
                width: dayNames.width / 7
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
        for (var i = 0; i < 7; ++i) {
            var day = (i + s) % 7
            dayNamesModel.append({
                "dayName" : d.locale.dayName(day, Locale.ShortFormat),
                "holiday" : day == 0 || day == 6
            })
        }
    }

    onMondayIsFirstDayChanged: updateDayNames()
    Component.onCompleted: updateDayNames()
}

