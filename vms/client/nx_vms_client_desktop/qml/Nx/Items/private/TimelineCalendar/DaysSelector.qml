// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import Nx 1.0
import nx.vms.client.core 1.0

import "date_utils.js" as DateUtils

Item
{
    id: control

    property var range: NxGlobals.dateRange(
        new Date(1970, 1, 1), DateUtils.addDays(d.today, 1))
    property var selection: NxGlobals.dateRange(d.today, DateUtils.addDays(d.today, 1))

    property alias displayOffset: calendarModel.displayOffset
    property alias locale: calendarModel.locale
    property alias visibleYear: calendarModel.year
    property int visibleMonth: 0
    property alias periodStorage: calendarModel.periodStorage
    property alias allCamerasPeriodStorage: calendarModel.allCamerasPeriodStorage
    property bool periodsVisible: true

    QtObject
    {
        id: d

        readonly property date today: DateUtils.stripTime(
            new Date(new Date().getTime() + displayOffset))

        readonly property date rangeStartDay:
            DateUtils.stripTime(range.start)
        readonly property date rangeEndDay:
            DateUtils.stripTime(DateUtils.addMilliseconds(range.end, -1))

        readonly property date selectionStartDay:
            DateUtils.stripTime(selection.start)
        readonly property date selectionEndDay:
            DateUtils.stripTime(DateUtils.addMilliseconds(selection.end, -1))

        function setSelection(start, end, endClicked)
        {
            selection = NxGlobals.dateRange(start, end)

            const clickedDate = endClicked ? selection.end : selection.start
            visibleYear = clickedDate.getFullYear()
            visibleMonth = clickedDate.getMonth()
        }
    }

    Grid
    {
        id: grid

        anchors.fill: parent

        columns: 7
        property real cellWidth: width / columns
        property real cellHeight: height / 6

        Repeater
        {
            id: repeater

            model: CalendarModel
            {
                id: calendarModel
                // Javascript counts months from 0 when Qt counts them from 1.
                month: control.visibleMonth + 1
            }

            Item
            {
                id: calendarDay

                readonly property date date: model.date

                readonly property bool current: DateUtils.areDatesEqual(date, d.today)

                width: grid.cellWidth
                height: grid.cellHeight

                enabled: date >= d.rangeStartDay && date <= d.rangeEndDay

                SelectionMarker
                {
                    anchors
                    {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }

                    start: DateUtils.areDatesEqual(date, d.selectionStartDay)
                    end: DateUtils.areDatesEqual(date, d.selectionEndDay)
                    visible: date >= d.selectionStartDay && date <= d.selectionEndDay
                }

                Rectangle
                {
                    anchors
                    {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }

                    radius: 2
                    color: "transparent"
                    border.width: 1
                    border.color: ColorTheme.colors.light16

                    visible: mouseArea.containsMouse
                }

                Rectangle
                {
                    anchors.centerIn: dayLabel

                    width: parent.width - 16
                    height: parent.height - 14

                    radius: 2
                    visible: calendarDay.current
                    color: ColorTheme.colors.light4
                }

                Text
                {
                    id: dayLabel

                    y: 4
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: calendarDay.date.getDate()
                    color:
                    {
                        if (calendarDay.current)
                            return ColorTheme.colors.dark9

                        return (enabled && model.date.getUTCMonth() === visibleMonth)
                            ? ColorTheme.text
                            : ColorTheme.transparent(ColorTheme.colors.light16, 0.5)
                    }

                    topPadding: 1
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                ArchiveIndicator
                {
                    cameraHasArchive: periodsVisible && model.hasArchive
                    anyCameraHasArchive: model.anyCameraHasArchive
                }

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: function(event)
                    {
                        if (!(event.modifiers & Qt.ControlModifier))
                        {
                            d.setSelection(
                                calendarDay.date, DateUtils.addDays(calendarDay.date, 1))
                            return
                        }

                        if (calendarDay.date < selection.start)
                        {
                            d.setSelection(calendarDay.date, selection.end)
                        }
                        else
                        {
                            d.setSelection(
                                selection.start, DateUtils.addDays(calendarDay.date, 1), true)
                        }
                    }
                }
            }
        }
    }
}
