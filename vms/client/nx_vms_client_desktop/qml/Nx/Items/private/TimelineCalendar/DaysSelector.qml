// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import nx.vms.client.core
import nx.vms.common

import "date_utils.js" as DateUtils

Item
{
    id: control

    property var range: NxGlobals.timePeriodFromInterval(
        new Date(1970, 1, 1),
        DateUtils.addDays(d.today, 1))

    property var selection: NxGlobals.timePeriodFromInterval(
        d.today,
        DateUtils.addDays(d.today, 1))

    property alias timeZone: calendarModel.timeZone
    property date currentDate: new Date()
    property alias locale: calendarModel.locale
    property alias visibleYear: calendarModel.year
    property int visibleMonth: 0
    property alias timePeriodType: calendarModel.timePeriodType
    property alias periodStorage: calendarModel.periodStorage
    property alias allCamerasPeriodStorage: calendarModel.allCamerasPeriodStorage
    property bool periodsVisible: true

    implicitHeight: 32 * 6

    QtObject
    {
        id: d

        readonly property date today: DateUtils.stripTime(currentDate)

        function setSelection(start, end, endClicked)
        {
            selection = NxGlobals.timePeriodFromInterval(
                Math.max(start, range.startTimeMs),
                Math.min(end, range.endTimeMs))

            const clickedDate = endClicked ? selection.endTimeMs : selection.startTimeMs

            visibleYear = Number(NxGlobals.formatTimestamp(clickedDate, "yyyy", timeZone))
            visibleMonth = Number(NxGlobals.formatTimestamp(clickedDate, "MM", timeZone)) - 1
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

                readonly property QnTimePeriod dayRange: model.timeRange
                readonly property bool current: dayRange.contains(currentDate.getTime())

                width: grid.cellWidth
                height: grid.cellHeight

                enabled: range.intersects(dayRange)

                SelectionMarker
                {
                    id: selectionMarker

                    anchors
                    {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }

                    start: dayRange.contains(selection.startTimeMs)
                    end: dayRange.contains(selection.endTimeMs - 1)
                    visible: dayRange.intersects(selection)
                }

                Rectangle
                {
                    anchors.fill: selectionMarker
                    color: "transparent"
                    radius: 4
                    border.width: 1
                    border.color: mouseArea.containsMouse
                        ? ColorTheme.colors.light12
                        : calendarDay.current
                            ? ColorTheme.colors.brand_core
                            : "transparent"
                }

                Text
                {
                    id: dayLabel

                    y: 6
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: model.date.getDate()
                    color:
                    {
                        return (model.date.getMonth() === visibleMonth)
                            ? ColorTheme.colors.light4 : ColorTheme.colors.light16
                    }

                    topPadding: 1
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                ArchiveIndicator
                {
                    timePeriodType: control.timePeriodType
                    cameraHasArchive: periodsVisible && model.hasArchive
                    anyCameraHasArchive: periodsVisible && model.anyCameraHasArchive
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
                            d.setSelection(dayRange.startTimeMs, dayRange.endTimeMs)
                            return
                        }

                        if (dayRange.startTimeMs < selection.startTimeMs)
                        {
                            d.setSelection(dayRange.startTimeMs, selection.endTimeMs)
                        }
                        else
                        {
                            d.setSelection(selection.startTimeMs, dayRange.endTimeMs, true)
                        }
                    }
                }
            }
        }
    }
}
