import QtQuick 2.4
import QtQuick.Window 2.2

import com.networkoptix.qml 1.0

import "../controls"
import "../main.js" as Main

Item {
    id: calendar

    property int year: (new Date()).getFullYear()
    property int month: (new Date()).getMonth() + 1
    property date date: new Date()
    property bool mondayIsFirstDay: true
    property bool horizontal: Main.isMobile() ? !isPhone || Screen.primaryOrientation == Qt.LandscapeOrientation
                                              : mainWindow.width > dp(540)
    property var chunkProvider

    signal datePicked(date date)

    height: d.ui.height
    width: horizontal ? d.ui.width : parent.width

    QtObject {
        id: d

        readonly property var locale: Qt.locale()
        readonly property alias ui: uiLoader.item


        function monthDiff(date) {
            return (year - date.getFullYear()) * 12 + month - date.getMonth() - 1
        }

        function previousMonthClicked() {
            contentYAnimation.complete()
            contentYAnimation.from = ui.monthsList.contentY
            ui.monthsList.positionViewAtBeginning()
            contentYAnimation.to = ui.monthsList.contentY
            contentYAnimation.start()
        }

        function nextMonthClicked() {
            contentYAnimation.complete()
            contentYAnimation.from = ui.monthsList.contentY
            ui.monthsList.positionViewAtEnd()
            contentYAnimation.to = ui.monthsList.contentY
            contentYAnimation.start()
        }

        function shiftToNeighbourMonth() {
            if (ui.monthsList.atYBeginning) {
                if (month > 1) {
                    --month
                } else {
                    month = 12
                    --year
                }
                monthsModel.remove(2, 1)
                monthsModel.insert(0, { "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
                ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
            } else if (ui.monthsList.atYEnd) {
                if (month < 12) {
                    ++month
                } else {
                    month = 1
                    ++year
                }
                monthsModel.remove(0, 1)
                monthsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
            }
        }

        function populate() {
            monthsModel.clear()
            monthsModel.append({ "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
            monthsModel.append({ "year" : year, "month" : month })
            monthsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
        }
    }

    Loader {
        id: uiLoader
        sourceComponent: horizontal ? horizontalLayoutComponent : verticalLayoutComponent
    }

    Component {
        id: horizontalLayoutComponent

        QnHorizontalCalendarLayout {
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
            monthLabel.text: d.locale.monthName(month - 1)
            yearLabel.text: year
            dayNamesRow.mondayIsFirstDay: mondayIsFirstDay

            previousMonthButton.onClicked: d.previousMonthClicked()
            nextMonthButton.onClicked: d.nextMonthClicked()

            monthsList.onMovingChanged: {
                if (monthsList.moving)
                    return

                d.shiftToNeighbourMonth()
            }
            monthsList.onContentWidthChanged: {
                monthsList.positionViewAtIndex(1, ListView.Beginning)
            }
        }
    }

    Component {
        id: verticalLayoutComponent

        QnVerticalCalendarLayout {
            width: calendar.width
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
            monthLabel.text: d.locale.monthName(month - 1)
            yearLabel.text: year
            dayNamesRow.mondayIsFirstDay: mondayIsFirstDay

            previousMonthButton.onClicked: d.previousMonthClicked()
            nextMonthButton.onClicked: d.nextMonthClicked()

            monthsList.onMovingChanged: {
                if (monthsList.moving)
                    return

                d.shiftToNeighbourMonth()
            }
            monthsList.onContentWidthChanged: {
                monthsList.positionViewAtIndex(1, ListView.Beginning)
            }
        }
    }

    ListModel {
        id: monthsModel
    }

    Component {
        id: monthDelegate

        QnCalendarMonth {
            id: calendarMonth

            width: d.ui.monthsList.width
            height: d.ui.monthsList.height
            year: model.year
            month: model.month
            onCurrentDateChanged: calendar.date = currentDate
            onDatePicked: calendar.datePicked(date)
            mondayIsFirstDay: calendar.mondayIsFirstDay
            chunkProvider: calendar.chunkProvider
            dayHeight: horizontal ? dp(40) : dp(48)

            Binding {
                target: calendarMonth
                property: "currentDate"
                value: calendar.date
            }
        }
    }

    NumberAnimation {
        id: contentYAnimation

        target: d.ui.monthsList
        property: "contentY"

        duration: 500
        easing.type: Easing.OutCubic
        running: false

        onStopped: d.shiftToNeighbourMonth()
    }

    onDateChanged: {
        var dm = d.monthDiff(date)

        if (dm === 0)
            return

        if (dm === -1) {
            d.ui.monthsList.positionViewAtEnd()
            d.shiftToNeighbourMonth()
        } else if (dm === 1) {
            d.ui.monthsList.positionViewAtBeginning()
            d.shiftToNeighbourMonth()
        } else {
            year = date.getFullYear()
            month = date.getMonth() + 1

            d.populate()
            d.ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
        }
    }

    Component.onCompleted: d.populate()
}

