// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml
import QtQuick.Window
import QtQuick.Controls

import Nx.Core

import nx.vms.client.core

import "private/ArchiveCalendar"

Pane
{
    id: control

    property real position: 0
    property real displayOffset: 0
    property ChunkProvider chunkProvider: null
    property bool horizontal: mainWindow.width > 540

    signal picked(real position)
    signal closeClicked()

    function resetToCurrentPosition()
    {
        d.monthData = d.createMonthDataFromPosition(control.position, control.displayOffset)
        monthsModel.clear()
        monthsModel.append(d.prevMonthData(d.monthData))
        monthsModel.append(d.monthData)
        monthsModel.append(d.nextMonthData(d.monthData))
        d.ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
    }

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
        property var monthData: createMonthDataFromDate(new Date())

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

        function shiftToNextMonth()
        {
            d.monthData = prevMonthData(d.monthData)

            monthsModel.remove(2, 1)
            monthsModel.insert(0, prevMonthData(d.monthData))
            ui.monthsList.positionViewAtIndex(1, ListView.Beginning)
        }

        function shiftToPrevMonth()
        {
            d.monthData = nextMonthData(d.monthData)

            monthsModel.remove(0, 1)
            monthsModel.append(nextMonthData(d.monthData))
        }

        function shiftToNeighbourMonth()
        {
            if (ui.monthsList.atYBeginning)
                shiftToNextMonth()
            else if (ui.monthsList.atYEnd)
                shiftToPrevMonth()
        }

        function createMonthData(year, month)
        {
            var result = {}
            result.year = year
            result.month = month
            return result;
        }

        function createMonthDataFromDate(date)
        {
            return createMonthData(date.getUTCFullYear(), date.getUTCMonth() + 1)
        }

        function createMonthDataFromPosition(position, displayOffset)
        {
            return createMonthDataFromDate(new Date(position + displayOffset))
        }

        function prevMonthData(data)
        {
            return createMonthData(
                data.month > 1 ? data.year : data.year - 1,
                data.month > 1 ? data.month - 1 : 12)
        }

        function nextMonthData(data)
        {
            return createMonthData(
                data.month < 12 ? data.year : data.year + 1,
                data.month < 12 ? data.month + 1 : 1)
        }
    }

    background: Rectangle
    {
        color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.95)
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
            locale: control.locale
            monthText: locale.standaloneMonthName(d.monthData.month - 1, Locale.LongFormat)
            yearText: d.monthData.year
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
        }
    }

    Component
    {
        id: verticalCalendarComponent

        VerticalCalendar
        {
            locale: control.locale
            monthText: locale.standaloneMonthName(d.monthData.month - 1, Locale.LongFormat)
            yearText: d.monthData.year
            monthsList.model: monthsModel
            monthsList.delegate: monthDelegate
        }
    }

    Connections
    {
        target: d.ui

        function onPreviousMonthClicked()
        {
            d.previousMonthClicked()
        }

        function onNextMonthClicked()
        {
            d.nextMonthClicked()
        }

        function onCloseClicked()
        {
            control.closeClicked()
        }
    }

    Connections
    {
        target: d.ui.monthsList
        ignoreUnknownSignals: true

        function onMovingChanged()
        {
            if (d.ui.monthsList.moving)
                return

            d.shiftToNeighbourMonth()
        }

        function onContentHeightChanged()
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

            position: control.position
            displayOffset: control.displayOffset
            width: d.ui.monthsList.width
            height: d.ui.monthsList.height
            year: model.year
            month: model.month
            onPicked: position => control.picked(position)
            locale: control.locale
            periodStorage: control.chunkProvider
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
}
