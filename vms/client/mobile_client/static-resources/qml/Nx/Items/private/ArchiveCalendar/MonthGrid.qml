// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import nx.vms.client.core 1.0
import nx.vms.common

Item
{
    id: control

    property real position: new Date()
    property alias timeZone: calendarModel.timeZone
    property alias locale: calendarModel.locale
    property alias year: calendarModel.year
    property alias month: calendarModel.month
    property alias periodStorage: calendarModel.periodStorage

    signal picked(real position)

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
            }

            Item
            {
                id: calendarDay

                readonly property QnTimePeriod dayRange: model.timeRange
                readonly property bool current: dayRange.contains(position)

                width: grid.cellWidth
                height: grid.cellHeight
                enabled: !model.isFutureDate && model.display

                Rectangle
                {
                    id: archiveHighlight

                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    width: parent.width + 3
                    height: 3
                    radius: height / 2
                    color: ColorTheme.colors.green_core
                    visible: model.hasArchive && model.display
                }

                Rectangle
                {
                    id: currentDate

                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    radius: width / 2
                    visible: calendarDay.current && model.display
                    color: ColorTheme.colors.light1
                }

                Text
                {
                    id: calendarDate

                    anchors.centerIn: parent
                    text: model.display
                    color:
                    {
                        if (calendarDay.current)
                            return ColorTheme.colors.dark3

                        return enabled ? ColorTheme.colors.light1 : ColorTheme.colors.dark15
                    }

                    font.pixelSize: 16
                }

                MouseArea
                {
                    id: clickArea

                    anchors.fill: parent
                    onClicked: control.picked(dayRange.startTimeMs)
                }
            }
        }
    }
}
