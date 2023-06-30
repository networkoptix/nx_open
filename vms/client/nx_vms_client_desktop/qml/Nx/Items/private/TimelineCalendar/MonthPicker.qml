// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml

import Nx.Core

import nx.vms.client.core

Item
{
    property alias year: monthListModel.year
    property alias displayOffset: monthListModel.displayOffset
    property alias locale: monthListModel.locale
    property alias periodStorage: monthListModel.periodStorage
    property alias allCamerasPeriodStorage: monthListModel.allCamerasPeriodStorage

    property int month: 0
    property int currentMonth: -1

    signal monthClicked(int month)

    Grid
    {
        id: grid

        anchors.fill: parent

        columns: 3
        rows: 4

        Repeater
        {
            model: MonthListModel
            {
                id: monthListModel
            }

            Rectangle
            {
                property bool current: month === model.month

                width: grid.width / grid.columns
                height: grid.height / grid.rows

                color: "transparent"
                border.width: 1
                border.color: monthButtonMouseArea.containsMouse
                    ? ColorTheme.colors.light12
                    : "transparent"

                radius: 2

                Text
                {
                    id: label

                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -4

                    text: locale.standaloneMonthName(model.month, Locale.ShortFormat)
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    font.capitalization: Font.AllUppercase

                    color: (model.month === currentMonth)
                        ? ColorTheme.highlight : ColorTheme.buttonText
                }

                ArchiveIndicator
                {
                    anchors {
                        top: label.bottom
                        topMargin: 4
                        bottom: undefined //< Override the default value.
                    }
                    width: 32
                    height: 4
                    cameraHasArchive: model.hasArchive
                    anyCameraHasArchive: model.anyCameraHasArchive
                }

                MouseArea
                {
                    id: monthButtonMouseArea

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked:
                    {
                        month = model.month
                        monthClicked(month)
                    }
                }
            }
        }
    }
}
