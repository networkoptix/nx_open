import QtQuick 2.0
import QtQuick.Window 2.0
import QtQuick.Controls 2.4
import Nx 1.0
import com.networkoptix.qml 1.0

import "private/ArchiveCalendar"

Pane
{
    id: calendar

    property int year: (new Date()).getFullYear()
    property int month: (new Date()).getMonth() + 1
    property date date: new Date()
    property QnCameraChunkProvider chunkProvider: null
    property bool horizontal: mainWindow.width > 540

    signal datePicked(date date)
    signal closeClicked()

    padding: 0

    implicitWidth: loader.implicitWidth
    implicitHeight: loader.implicitHeight

    ListModel
    {
        id: monthsModel
    }

    QtObject
    {
        id: d

        readonly property alias ui: loader.item

        function monthDiff(date)
        {
            return (year - date.getFullYear()) * 12 + month - date.getMonth() - 1
        }

        function previousMonthClicked()
        {
            contentAnimation.complete()
            contentAnimation.from = ui.monthsList.contentY
            ui.monthsList.positionViewAtBeginning()
            contentAnimation.to = ui.monthsList.contentY
            contentAnimation.start()
        }

        function nextMonthClicked()
        {
            contentAnimation.complete()
            contentAnimation.from = ui.monthsList.contentY
            ui.monthsList.positionViewAtEnd()
            contentAnimation.to = ui.monthsList.contentY
            contentAnimation.start()
        }

        function shiftToNeighbourMonth()
        {
            if (ui.monthsList.atYBeginning)
            {
                if (month > 1)
                {
                    --month
                }
                else
                {
                    month = 12
                    --year
                }
                monthsModel.remove(2, 1)
                monthsModel.insert(0, { "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
                ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
            }
            else if (ui.monthsList.atYEnd)
            {
                if (month < 12)
                {
                    ++month
                }
                else
                {
                    month = 1
                    ++year
                }
                monthsModel.remove(0, 1)
                monthsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
            }
        }

        function populate()
        {
            monthsModel.clear()
            monthsModel.append({ "year" : month > 1 ? year : year - 1, "month" : month > 1 ? month - 1 : 12 })
            monthsModel.append({ "year" : year, "month" : month })
            monthsModel.append({ "year" : month < 12 ? year : year + 1, "month" : month < 12 ? month + 1 : 1 })
            d.ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
        }
    }

    background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.base8, 0.95)
        layer.enabled: true
    }

    Loader
    {
        id: loader
        sourceComponent: horizontal ? horizontalCalendarComponent : verticalCalendarComponent
    }

    Component
    {
        id: horizontalCalendarComponent

        HorizontalCalendar
        {
            locale: calendar.locale
            monthText: locale.standaloneMonthName(month - 1, Locale.LongFormat)
            yearText: year
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
        }
    }

    Component
    {
        id: verticalCalendarComponent

        VerticalCalendar
        {
            locale: calendar.locale
            monthText: locale.standaloneMonthName(month - 1, Locale.LongFormat)
            yearText: year
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
        }
    }

    Connections
    {
        target: d.ui

        onPreviousMonthClicked: d.previousMonthClicked()
        onNextMonthClicked: d.nextMonthClicked()
        onCloseClicked: calendar.closeClicked()
    }

    Connections
    {
        target: d.ui.monthsList
        ignoreUnknownSignals: true

        onMovingChanged:
        {
            if (d.ui.monthsList.moving)
                return

            d.shiftToNeighbourMonth()
        }

        onContentHeightChanged:
        {
            d.ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
        }
    }

    Component
    {
        id: monthDelegate

        MonthGrid
        {
            id: calendarMonth

            width: d.ui.monthsList.width
            height: d.ui.monthsList.height
            year: model.year
            month: model.month
            onCurrentDateChanged: calendar.date = currentDate
            onDatePicked: calendar.datePicked(date)
            locale: calendar.locale
            chunkProvider: calendar.chunkProvider

            Binding
            {
                target: calendarMonth
                property: "currentDate"
                value: calendar.date
            }
        }
    }

    NumberAnimation
    {
        id: contentAnimation

        target: d.ui.monthsList
        property: "contentY"

        duration: 500
        easing.type: Easing.OutCubic
        running: false

        onStopped: d.shiftToNeighbourMonth()
    }

    onDateChanged:
    {
        var dm = d.monthDiff(date)

        if (dm === 0)
            return

        if (dm === -1)
        {
            d.ui.monthsList.positionViewAtEnd()
            d.shiftToNeighbourMonth()
        }
        else if (dm === 1)
        {
            d.ui.monthsList.positionViewAtBeginning()
            d.shiftToNeighbourMonth()
        }
        else
        {
            year = date.getFullYear()
            month = date.getMonth() + 1

            d.populate()
            d.ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
        }
    }

    Component.onCompleted: d.populate()
}
