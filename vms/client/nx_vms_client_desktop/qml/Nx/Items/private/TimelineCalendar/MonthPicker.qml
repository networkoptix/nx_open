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
                    ? ColorTheme.colors.light12
                    : current
                        ? ColorTheme.highlight
                        : "transparent"

                radius: 2

                Text
                {
                    anchors.centerIn: parent

                    text: locale.monthName(modelData, Locale.ShortFormat)
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    font.capitalization: Font.AllUppercase

                    color: ColorTheme.buttonText
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
