// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQml

import Nx 1.0

Item
{
    property int month: 0

    signal monthClicked(int month)

    Grid
    {
        id: grid

        anchors.fill: parent

        columns: 3
        rows: 4

        Repeater
        {
            model: 12

            Rectangle
            {
                property bool current: month === modelData

                width: grid.width / grid.columns
                height: grid.height / grid.rows

                color: "transparent"
                border.width: 1
                border.color: monthButtonMouseArea.containsMouse
                    ? ColorTheme.midlight : "transparent"
                radius: 2

                Text
                {
                    anchors.centerIn: parent

                    text: locale.monthName(modelData, Locale.ShortFormat)
                    font.pixelSize: 15
                    font.weight: Font.DemiBold

                    color: current ? ColorTheme.colors.dark8 : ColorTheme.colors.light4

                    Rectangle
                    {
                        anchors.centerIn: parent
                        width: parent.width + 16
                        height: parent.height + 8
                        z: -1
                        radius: 2
                        color: current ? ColorTheme.colors.light6 : "transparent"
                    }
                }

                MouseArea
                {
                    id: monthButtonMouseArea

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked:
                    {
                        month = modelData
                        monthClicked(month)
                    }
                }
            }
        }
    }
}
