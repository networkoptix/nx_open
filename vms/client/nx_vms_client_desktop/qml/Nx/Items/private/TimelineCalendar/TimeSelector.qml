// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop
import nx.vms.common

Item
{
    id: control

    property var locale: Qt.locale()
    property alias date: dayHoursModel.date

    property var range: NxGlobals.timePeriodFromInterval(
        new Date(1970, 1, 1),
        DateUtils.addDays(d.today, 1))

    property var selection: NxGlobals.timePeriodFromInterval(
        d.today,
        DateUtils.addDays(d.today, 1))

    property alias timeZone: dayHoursModel.timeZone
    property alias timePeriodType: dayHoursModel.timePeriodType
    property alias periodStorage: dayHoursModel.periodStorage
    property alias allCamerasPeriodStorage: dayHoursModel.allCamerasPeriodStorage
    property bool periodsVisible: true

    implicitHeight: grid.implicitHeight

    signal selectionEdited(var start, var end)

    DayHoursModel
    {
        id: dayHoursModel
        amPmTime: !!locale.timeFormat(Locale.LongFormat).match(/[aA]/) //< Find AM/PM indicator.
    }

    Column
    {
        id: amPmIndicator

        spacing: 1
        visible: dayHoursModel.amPmTime

        component AmPmLabel: Text
        {
            height: 32
            width: control.width / (grid.columns + 1)
            text: locale.amText
            color: enabled ? ColorTheme.colors.light4 : ColorTheme.colors.light16
            font.pixelSize: 10
            font.weight: Font.Medium
            topPadding: 7
        }
        AmPmLabel { text: locale.amText }
        AmPmLabel { text: locale.pmText }
    }

    Grid
    {
        id: grid

        x: amPmIndicator.visible ? amPmIndicator.width : 0
        width: parent.width - x
        height: parent.height

        columns: 12

        readonly property real cellWidth: width / columns

        Repeater
        {
            model: dayHoursModel

            Item
            {
                id: hourItem

                property int hour: model.hour

                readonly property QnTimePeriod dayRange: dayHoursModel.dayRange
                readonly property QnTimePeriod hourRange: model.timeRange

                width: grid.cellWidth
                height: 32

                enabled: hourRange.intersects(range)
                    && dayRange.contains(selection)
                    && model.isHourValid

                GlobalToolTip.text:
                    model.isHourValid ? "" : qsTr("Time is unavailable due to DST changes")

                SelectionMarker
                {
                    id: selectionMarker

                    anchors
                    {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }

                    start: hourRange.contains(selection.startTimeMs)
                    end: hourRange.contains(selection.endTimeMs - 1)
                    visible: enabled && hourRange.intersects(selection)
                }

                Rectangle
                {
                    anchors.fill: selectionMarker
                    color: "transparent"
                    radius: 4
                    border.width: 1
                    border.color: ColorTheme.colors.light12
                    visible: enabled && mouseArea.containsMouse
                }

                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 7
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    text: model.display
                    color: enabled ? ColorTheme.colors.light4 : ColorTheme.colors.light16
                }

                ArchiveIndicator
                {
                    width: 12
                    timePeriodType: control.timePeriodType
                    cameraHasArchive: periodsVisible && model.hasArchive
                    anyCameraHasArchive: periodsVisible && model.anyCameraHasArchive
                    visible: enabled
                }

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onPressed: event =>
                    {
                        if (!(event.modifiers & Qt.ControlModifier))
                        {
                            selectionEdited(hourRange.startTimeMs, hourRange.endTimeMs)
                            return
                        }

                        if (hourRange.startTimeMs < selection.startTimeMs)
                            selectionEdited(hourRange.startTimeMs, selection.endTimeMs)
                        else
                            selectionEdited(selection.startTimeMs, hourRange.endTimeMs)
                    }
                }
            }
        }
    }
}
