import QtQuick 2.4

import com.networkoptix.qml 1.0

import "../controls"

Item {
    id: calendar

    property int year: (new Date()).getFullYear()
    property int month: (new Date()).getMonth() + 1
    property date date: new Date()
    property bool mondayIsFirstDay: true
    property var chunkProvider

    signal datePicked(date date)

    height: content.height

    property var _locale: Qt.locale()

    Column {
        id: content
        width: parent.width

        Item {
            id: header

            height: dp(48)
            width: parent.width

            Text {
                id: monthYearLabel
                anchors.centerIn: parent
                color: QnTheme.calendarText
                font.pixelSize: sp(16)
                font.weight: Font.DemiBold

                function update() {
                    if (_locale)
                        text = _locale.monthName(month - 1) + " " + year
                }
            }

            QnIconButton {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                icon: "image://icon/arrow_left.png"
                explicitRadius: dp(24)
                onClicked: {
                    contentYAnimation.complete()
                    contentYAnimation.from = calendarsList.contentY
                    calendarsList.positionViewAtBeginning()
                    contentYAnimation.to = calendarsList.contentY
                    contentYAnimation.start()
                }
            }

            QnIconButton {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                icon: "image://icon/arrow_right.png"
                explicitRadius: dp(24)
                onClicked: {
                    contentYAnimation.complete()
                    contentYAnimation.from = calendarsList.contentY
                    calendarsList.positionViewAtEnd()
                    contentYAnimation.to = calendarsList.contentY
                    contentYAnimation.start()
                }
            }
        }

        Row {
            id: dayNames

            height: dp(56)
            width: parent.width

            Repeater {
                model: ListModel {
                    id: dayNamesModel
                }

                Item {
                    width: calendarsList.width / 7
                    height: parent.height

                    Text {
                        anchors.centerIn: parent
                        text: model.dayName
                        color: model.holiday ? QnTheme.calendarHolidayName : QnTheme.calendarDayName
                        font.pixelSize: sp(13)
                        font.weight: Font.Light
                    }
                }
            }

            function updateDayNames() {
                dayNamesModel.clear()

                dayNamesModel.append({ "dayName" : qsTr("Tue"), "holiday" : false })
                dayNamesModel.append({ "dayName" : qsTr("Wed"), "holiday" : false })
                dayNamesModel.append({ "dayName" : qsTr("Thu"), "holiday" : false })
                dayNamesModel.append({ "dayName" : qsTr("Fri"), "holiday" : false })
                dayNamesModel.append({ "dayName" : qsTr("Sat"), "holiday" : true })

                if (mondayIsFirstDay) {
                    dayNamesModel.append({ "dayName" : qsTr("Sun"), "holiday" : true })
                    dayNamesModel.insert(0, { "dayName" : qsTr("Mon"), "holiday" : false })
                } else {
                    dayNamesModel.append({ "dayName" : qsTr("Mon"), "holiday" : false })
                    dayNamesModel.insert(0, { "dayName" : qsTr("Sun"), "holiday" : true })
                }
            }
        }

        QnListView {
            id: calendarsList

            width: parent.width
            height: width / 7 * 6
            clip: true

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapOneItem

            model: ListModel {
                id: calendarsModel
            }

            delegate: QnCalendarMonth {
                id: calendarMonth

                width: calendarsList.width
                height: calendarsList.height
                year: model.year
                month: model.month
                onCurrentDateChanged: calendar.date = currentDate
                onDatePicked: calendar.datePicked(date)
                mondayIsFirstDay: calendar.mondayIsFirstDay
                chunkProvider: calendar.chunkProvider

                Binding {
                    target: calendarMonth
                    property: "currentDate"
                    value: calendar.date
                }
            }

            onMovingChanged: {
                if (moving)
                    return

                shiftToNeighbourMonth()
            }

            onContentWidthChanged: {
                positionViewAtIndex(1, ListView.Beginning)
            }

            NumberAnimation on contentY {
                id: contentYAnimation
                duration: 500
                easing.type: Easing.OutCubic
                running: false

                onStopped: calendarsList.shiftToNeighbourMonth()
            }

            function shiftToNeighbourMonth() {
                if (atYBeginning) {
                    if (month > 1) {
                        --month
                    } else {
                        month = 12
                        --year
                    }
                    calendarsModel.remove(2, 1)
                    calendarsModel.insert(0, { "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
                    positionViewAtIndex(1, ListView.Beginning)
                } else if (atYEnd) {
                    if (month < 12) {
                        ++month
                    } else {
                        month = 1
                        ++year
                    }
                    calendarsModel.remove(0, 1)
                    calendarsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
                }
            }
        }
    }

    function monthDiff(date) {
        return (year - date.getFullYear()) * 12 + month - date.getMonth() - 1
    }

    onYearChanged: {
        monthYearLabel.update()
    }

    onMonthChanged: {
        monthYearLabel.update()
    }

    onDateChanged: {
        var dm = monthDiff(date)

        if (dm === 0)
            return

        if (dm === -1) {
            calendarsList.positionViewAtEnd()
            calendarsList.shiftToNeighbourMonth()
        } else if (dm === 1) {
            calendarsList.positionViewAtBeginning()
            calendarsList.shiftToNeighbourMonth()
        } else {
            year = date.getFullYear()
            month = date.getMonth() + 1

            populate()
            calendarsList.positionViewAtIndex(1, ListView.Beginning)
        }
    }

    Component.onCompleted: {
        dayNames.updateDayNames()
        populate()
        monthYearLabel.update()
    }

    function populate() {
        calendarsModel.clear()
        calendarsModel.append({ "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
        calendarsModel.append({ "year" : year, "month" : month })
        calendarsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
    }
}

