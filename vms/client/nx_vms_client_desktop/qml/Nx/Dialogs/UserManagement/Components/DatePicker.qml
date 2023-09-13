// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls

RowLayout
{
    id: control

    spacing: 2
    property date currentDate: new Date()

    property int displayOffset: 0 //< In milliseconds.

    // We use different locales to get the region settings (like first day of week) and to get the
    // translated names (days of week, months).
    property var regionLocale: locale

    property var locale: Qt.locale()

    property bool monthPickerVisible: false

    property int visibleMonth
    property int visibleYear

    function getDateForDisplay()
    {
        return new Date(control.currentDate.getTime() + control.displayOffset)
    }

    function generateDate(year, month, day)
    {
        return new Date(
            (new Date(year, month, day, 23, 59, 59)).getTime() - control.displayOffset)
    }

    DateField
    {
        id: dateField

        minimum: new Date(monthGrid.nowDate.getTime() + control.displayOffset)

        Binding on date { value: getDateForDisplay() }

        onDateEdited: (date) =>
        {
            control.currentDate = generateDate(date.getFullYear(), date.getMonth(), date.getDate())
        }
    }

    ImageButton
    {
        id: calendarButton

        width: 20
        height: 20

        hoverEnabled: control.enabled
        enabled: !popup.opened

        icon.source: "image://svg/skin/calendar/calendar.svg"
        icon.width: width
        icon.height: height
        icon.color: hovered
            ? ColorTheme.colors.light13
            : popup.opened ? ColorTheme.colors.light17 : ColorTheme.colors.light16

        background: null

        onClicked:
        {
            const displayDate = getDateForDisplay()
            control.visibleMonth = displayDate.getMonth()
            control.visibleYear = displayDate.getFullYear()

            popup.open()
        }
    }

    Popup
    {
        id: popup

        parent: dateField
        x: dateField.width + calendarButton.width / 2 + 2 - width / 2
        y: dateField.y + dateField.height

        background: null
        padding: 0

        PopupShadow
        {
            source: calendarBack
        }

        contentItem: Control
        {
            implicitWidth: 324
            implicitHeight: contentItem.implicitHeight + padding * 2
            padding: 8

            background: Rectangle
            {
                id: calendarBack

                color: ColorTheme.colors.dark11
                radius: 2
            }

            contentItem: Column
            {
                spacing: 0

                component Splitter: Rectangle
                {
                    width: parent.width
                    height: 1
                    color: ColorTheme.colors.dark9
                }

                component Spacer: Item
                {
                    width: 1
                    height: 8
                }

                RowLayout
                {
                    id: header

                    width: parent.width
                    height: 32

                    component ArrowButton: ImageButton
                    {
                        width: 20
                        height: 32

                        icon.width: 20
                        icon.height: 20
                        focusPolicy: Qt.NoFocus

                        background: null
                        icon.color: hovered ? ColorTheme.text : ColorTheme.light
                    }

                    ArrowButton
                    {
                        icon.source: "image://svg/skin/calendar/arrow_left.svg"
                        objectName: "backArrowButton"

                        readonly property int kMinYear: 1970

                        enabled: monthPickerVisible
                            ? visibleYear > kMinYear
                            : visibleYear > kMinYear || visibleMonth > 1

                        onClicked:
                        {
                            visibleMonth = (visibleMonth + 12 - 1) % 12
                            if (visibleMonth === 11)
                                --visibleYear
                        }
                    }

                    Text
                    {
                        id: yearMonthLabel

                        Layout.fillWidth: true

                        height: 32

                        font.pixelSize: 16
                        font.weight: Font.Medium
                        font.capitalization: Font.Capitalize
                        color: ColorTheme.text
                        text: `${locale.standaloneMonthName(visibleMonth)} ${visibleYear}`

                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                    }

                    ArrowButton
                    {
                        icon.source: "image://svg/skin/calendar/arrow_right.svg"
                        objectName: "forwardArrowButton"

                        onClicked:
                        {
                            visibleMonth = (visibleMonth + 1) % 12
                            if (visibleMonth === 0)
                                ++visibleYear
                        }
                    }
                }

                Spacer {}

                Splitter {}

                DayOfWeekRow
                {
                    id: dayOfWeekRow
                    width: parent.width
                    height: 24
                    spacing: 0
                    locale: control.regionLocale

                    delegate: Text
                    {
                        text: control.locale.standaloneDayName(model.day, Locale.ShortFormat)
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        font.capitalization: Font.AllUppercase
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        color: control.locale.weekDays.indexOf(model.day) == -1
                            ? ColorTheme.text
                            : ColorTheme.windowText
                    }
                }

                Splitter {}

                Spacer {}

                MonthGrid
                {
                    id: monthGrid

                    width: parent.width
                    month: control.visibleMonth
                    year: control.visibleYear
                    locale: control.regionLocale
                    spacing: 0

                    property var selectedDate

                    property date nowDate: new Date()

                    Timer
                    {
                        interval: 1000
                        running: true
                        repeat: true
                        onTriggered: monthGrid.nowDate = new Date()
                    }

                    readonly property date today:
                    {
                        const serverToday = new Date(nowDate.getTime() - control.displayOffset)
                        return generateDate(
                            serverToday.getFullYear(), serverToday.getMonth(), serverToday.getDate())
                    }

                    delegate: Rectangle
                    {
                        id: cell

                        color: cellText.isSelected
                            ? ColorTheme.colors.dark13
                            : ColorTheme.colors.dark11

                        radius: 2

                        border.color: cellMouseArea.containsMouse
                            ? ColorTheme.colors.dark15
                            : "transparent"
                        border.width: cellMouseArea.containsMouse ? 1 : 0

                        implicitWidth: cellText.implicitWidth
                        implicitHeight: 32

                        required property var model

                        readonly property date cellDate: generateDate(
                            model.year, model.month, model.day)

                        readonly property bool isToday:
                            cellDate.getTime() == monthGrid.today.getTime()

                        Text
                        {
                            id: cellText

                            anchors.fill: parent

                            readonly property bool isSelected:
                                cell.cellDate.getTime() == control.currentDate.getTime()

                            readonly property bool isInPast:
                                cell.cellDate.getTime() < monthGrid.nowDate.getTime()

                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            text: model.day
                            font: Qt.font({pixelSize: 12, weight: Font.Medium})

                            color: cell.isToday
                                ? ColorTheme.colors.brand_core
                                : (isInPast || model.month !== monthGrid.month)
                                    ? ColorTheme.colors.light16
                                    : ColorTheme.text

                            MouseArea
                            {
                                id: cellMouseArea

                                enabled: !cellText.isInPast
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked:
                                {
                                    control.currentDate = cell.cellDate
                                    popup.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
