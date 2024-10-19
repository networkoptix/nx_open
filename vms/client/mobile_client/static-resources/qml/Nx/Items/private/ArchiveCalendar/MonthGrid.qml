// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import nx.vms.client.core 1.0

Item
{
    id: control

    property real position: new Date()
    property alias displayOffset: calendarModel.displayOffset
    property alias locale: calendarModel.locale
    property alias year: calendarModel.year
    property alias month: calendarModel.month
    property alias periodStorage: calendarModel.periodStorage

    signal picked(real position)

    readonly property date positionDate: new Date(position + displayOffset)

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

                property bool current:
                    model.date.getFullYear() === positionDate.getFullYear()
                        && model.date.getMonth() === positionDate.getMonth()
                        && model.date.getDate() === positionDate.getDate()

                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle
                {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    width: parent.width + 3
                    height: 3
                    radius: height / 2
                    color: ColorTheme.colors.green_core
                    visible: model.hasArchive
                }

                Rectangle
                {
                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    radius: width / 2
                    visible: calendarDay.current
                    color: ColorTheme.windowText
                }

                Text
                {
                    anchors.centerIn: parent
                    text: model.display
                    color:
                    {
                        if (calendarDay.current)
                            return ColorTheme.colors.dark3

                        return clickArea.enabled ? ColorTheme.windowText : ColorTheme.colors.dark15
                    }

                    font.pixelSize: 16
                }

                MouseArea
                {
                    id: clickArea

                    anchors.fill: parent
                    enabled: model.date <= new Date()
                    onClicked: control.picked(model.date.getTime() - displayOffset)
                }
            }
        }
    }
}
