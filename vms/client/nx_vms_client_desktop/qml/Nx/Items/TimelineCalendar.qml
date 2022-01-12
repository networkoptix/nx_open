// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls

import Nx 1.0
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

    property bool monthPickerVisible: false

    implicitWidth: 268
    implicitHeight: contentItem.implicitHeight

    background: Rectangle
    {
        color: ColorTheme.colors.dark4
        radius: 2
    }

    contentItem: Column
    {
        component Splitter: Rectangle
        {
            x: 8
            width: parent.width - 2 * x
            height: 1
            color: ColorTheme.colors.dark13
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
                leftPadding: 8
                rightPadding: 8
                topPadding: 8
                bottomPadding: 8

                flat: true
                backgroundColor: "transparent"
                textColor: hovered ? ColorTheme.text : ColorTheme.midlight
            }

            ArrowButton
            {
                x: 10
                iconUrl: "image://svg/skin/misc/corner_arrow_back.svg"
                onReleased:
                {
                    if (monthPickerVisible)
                        visibleYear = Math.max(1970, visibleYear - 1)
                    else
                        visibleMonth = new Date(visibleYear, visibleMonth - 1, 1).getMonth()
                }
            }
            ArrowButton
            {
                x: parent.width - width - 10
                iconUrl: "image://svg/skin/misc/corner_arrow_forward.svg"
                onReleased:
                {
                    if (monthPickerVisible)
                        ++visibleYear
                    else
                        visibleMonth = new Date(visibleYear, visibleMonth + 1, 1).getMonth()
                }
            }

            Text
            {
                id: yearMonthLabel

                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                height: 32

                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: yearMonthLabelMouseArea.containsMouse
                    ? ColorTheme.brightText : ColorTheme.text
                text: monthPickerVisible
                    ? visibleYear
                    : `${locale.monthName(visibleMonth)} ${visibleYear}`
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

                locale: control.locale

                delegate: Text
                {
                    text: model.shortName
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
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
                height: 28 * 6

                locale: control.locale

                visibleYear: selection.start.getFullYear()
                visibleMonth: selection.start.getMonth()
            }

            Spacer {}

            Splitter { color: ColorTheme.colors.dark6 }
        }

        MonthPicker
        {
            id: monthPicker

            x: 8
            width: parent.width - 2 * x
            height: calendar.height
            visible: monthPickerVisible

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

        Spacer {}

        TimeSelector
        {
            id: timeSelector

            readonly property date visualSelectionEnd:
                DateUtils.addMilliseconds(control.selection.end, -1)

            x: 8
            width: parent.width - 2 * x

            locale: control.locale
            date: control.selection.start
            visible: DateUtils.areDatesEqual(control.selection.start, visualSelectionEnd)

            minimumHour: DateUtils.areDatesEqual(selection.start, range.start)
                ? range.start.getHours() : 0

            maximumHour: DateUtils.areDatesEqual(visualSelectionEnd, range.end)
                ? range.end.getHours() : 23

            periodStorage: daysSelector.periodStorage
            allCamerasPeriodStorage: daysSelector.allCamerasPeriodStorage
            periodsVisible: daysSelector.periodsVisible
            displayOffset: daysSelector.displayOffset

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
            }
        }

        Item
        {
            width: parent.width
            height: timeSelector.height
            visible: !timeSelector.visible

            Image
            {
                anchors.centerIn: parent
                source: "image://svg/skin/misc/clock.svg"
                sourceSize: Qt.size(32, 32)
            }
        }

        Spacer { height: 6 }

        QuickIntervalPanel
        {
            width: parent.width

            onIntervalSelected: interval =>
            {
                selection = NxGlobals.dateRange(
                    new Date(range.end.getTime() - interval), range.end)
                visibleMonth = range.end.getMonth()
            }
        }
    }
}
