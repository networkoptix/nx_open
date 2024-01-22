// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import "private/TimelineCalendar"
import "private/TimelineCalendar/date_utils.js" as DateUtils

Control
{
    id: control

    property alias range: daysSelector.range
    property alias selection: daysSelector.selection
    property alias visibleYear: daysSelector.visibleYear
    property alias visibleMonth: daysSelector.visibleMonth
    property alias periodStorage: daysSelector.periodStorage
    property alias allCamerasPeriodStorage: daysSelector.allCamerasPeriodStorage
    property alias periodsVisible: daysSelector.periodsVisible
    property alias displayOffset: daysSelector.displayOffset
    property alias timeZone: daysSelector.timeZone

    // We use different locales to get the region settings (like first day of week) and to get the
    // translated names (days of week, months).
    property var regionLocale: locale

    property bool monthPickerVisible: false

    implicitWidth: 324
    implicitHeight: contentItem.implicitHeight

    property date currentDate: new Date(new Date().getTime() + displayOffset)

    Timer
    {
        interval: 1000
        running: true
        repeat: true
        onTriggered: currentDate = new Date(new Date().getTime() + displayOffset)
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark5
        radius: 2
    }

    contentItem: Column
    {
        component Splitter: Rectangle
        {
            x: 8
            width: parent.width - 2 * x
            height: 2
            color: ColorTheme.colors.dark6

            Rectangle
            {
                width: parent.width
                height: 1
                color: ColorTheme.colors.dark4
            }
        }

        component Spacer: Item
        {
            width: 1
            height: 8
        }

        width: parent.width

        Item
        {
            id: header

            width: parent.width
            height: 52

            component ArrowButton: Button
            {
                property bool forward: false

                y: 6
                width: 36
                height: 36
                leftPadding: 13
                rightPadding: 13
                topPadding: 13
                bottomPadding: 13
                icon.width: 10
                icon.height: 10
                focusPolicy: Qt.NoFocus

                flat: true
                backgroundColor: "transparent"
                textColor: hovered ? ColorTheme.text : ColorTheme.light
            }

            ArrowButton
            {
                x: 10
                iconUrl: "image://svg/skin/misc/corner_arrow_back.svg"
                id: "backArrowButton"

                readonly property int kMinYear: 1970

                enabled: monthPickerVisible
                    ? visibleYear > kMinYear
                    : visibleYear > kMinYear || visibleMonth > 1

                onReleased:
                {
                    if (monthPickerVisible)
                    {
                        visibleYear = Math.max(kMinYear, visibleYear - 1)
                    }
                    else
                    {
                        visibleMonth = (visibleMonth + 12 - 1) % 12
                        if (visibleMonth === 11)
                            --visibleYear
                    }
                }
            }
            ArrowButton
            {
                x: parent.width - width - 10
                iconUrl: "image://svg/skin/misc/corner_arrow_forward.svg"
                id: "forwardArrowButton"

                onReleased:
                {
                    if (monthPickerVisible)
                    {
                        ++visibleYear
                    }
                    else
                    {
                        visibleMonth = (visibleMonth + 1) % 12
                        if (visibleMonth === 0)
                            ++visibleYear
                    }
                }
            }

            Text
            {
                id: yearMonthLabel

                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                height: 32

                font.pixelSize: 16
                font.weight: Font.Medium
                font.capitalization: Font.AllUppercase
                color: yearMonthLabelMouseArea.containsMouse
                    ? ColorTheme.brightText : ColorTheme.text
                text: monthPickerVisible
                    ? visibleYear
                    : `${locale.standaloneMonthName(visibleMonth)} ${visibleYear}`
                verticalAlignment: Text.AlignVCenter

                MouseArea
                {
                    id: yearMonthLabelMouseArea
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: monthPickerVisible = !monthPickerVisible
                }
            }
        }

        Splitter {}

        Column
        {
            id: calendar

            width: parent.width
            visible: !monthPickerVisible

            DayOfWeekRow
            {
                x: 8
                width: parent.width - 2 * x
                height: 20

                locale: control.regionLocale

                delegate: Text
                {
                    text: control.locale.standaloneDayName(model.day, Locale.ShortFormat)
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    font.capitalization: Font.AllUppercase
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    color: locale.weekDays.indexOf(model.day) == -1
                        ? ColorTheme.text : ColorTheme.windowText
                }
            }

            Splitter {}

            Spacer {}

            DaysSelector
            {
                id: daysSelector

                x: 8
                width: parent.width - 2 * x

                locale: control.regionLocale
                currentDate: control.currentDate

                visibleYear: selection.start.getFullYear()
                visibleMonth: selection.start.getMonth()
            }

            Spacer {}

            Splitter {}

            Spacer {}

            TimeSelector
            {
                id: timeSelector

                readonly property date visualSelectionEnd:
                    DateUtils.addMilliseconds(control.selection.end, -1)

                x: 21
                width: parent.width - 2 * x

                locale: control.regionLocale
                date: control.selection.start

                minimumHour: DateUtils.areDatesEqual(selection.start, range.start)
                    ? range.start.getHours() : 0

                maximumHour: DateUtils.areDatesEqual(visualSelectionEnd, range.end)
                    ? range.end.getHours() : 23

                periodStorage: daysSelector.periodStorage
                allCamerasPeriodStorage: daysSelector.allCamerasPeriodStorage
                periodsVisible: daysSelector.periodsVisible
                displayOffset: daysSelector.displayOffset
                timeZone: daysSelector.timeZone

                enabled: DateUtils.areDatesEqual(selection.start, visualSelectionEnd)

                Binding
                {
                    target: timeSelector
                    property: "selectionStart"
                    value: control.selection.start.getHours()
                }
                Binding
                {
                    target: timeSelector
                    property: "selectionEnd"
                    value: timeSelector.visualSelectionEnd.getHours()
                }

                onSelectionEdited: (start, end) =>
                {
                    control.selection = NxGlobals.dateRange(
                        new Date(
                            control.selection.start.getFullYear(),
                            control.selection.start.getMonth(),
                            control.selection.start.getDate(),
                            start, 0, 0),
                        new Date(
                            visualSelectionEnd.getFullYear(),
                            visualSelectionEnd.getMonth(),
                            visualSelectionEnd.getDate(),
                            end + 1, 0, 0)
                    )
                    control.visibleMonth = control.selection.start.getMonth()
                    control.visibleYear = control.selection.start.getFullYear()
                }
            }
        }

        MonthPicker
        {
            id: monthPicker

            x: 8
            width: parent.width - 2 * x
            height: calendar.height
            visible: monthPickerVisible

            periodStorage: daysSelector.periodStorage
            allCamerasPeriodStorage: daysSelector.allCamerasPeriodStorage
            locale: control.locale
            displayOffset: daysSelector.displayOffset

            year: visibleYear
            currentMonth: (visibleYear === currentDate.getFullYear()) ? currentDate.getMonth() : -1

            Binding
            {
                target: monthPicker
                property: "month"
                value: visibleMonth
            }

            onMonthClicked: month =>
            {
                visibleMonth = month
                monthPickerVisible = false
            }
        }

        Spacer { height: 6 }

        QuickIntervalPanel
        {
            width: parent.width
            range: daysSelector.range
            currentDate: control.currentDate

            onIntervalSelected: startDate =>
            {
                selection = NxGlobals.dateRange(startDate, range.end)
                visibleYear = range.end.getFullYear()
                visibleMonth = range.end.getMonth()
            }
        }
    }
}
