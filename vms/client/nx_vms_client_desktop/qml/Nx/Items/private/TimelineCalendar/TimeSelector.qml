// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import nx.vms.client.core 1.0
import Nx 1.0

Item
{
    id: control

    property var locale: Qt.locale()
    property alias date: dayHoursModel.date

    property int minimumHour: 0
    property int maximumHour: 23

    property int selectionStart: minimumHour
    property int selectionEnd: maximumHour

    property alias displayOffset: dayHoursModel.displayOffset
    property alias periodStorage: dayHoursModel.periodStorage
    property alias allCamerasPeriodStorage: dayHoursModel.allCamerasPeriodStorage
    property bool periodsVisible: true

    implicitHeight: 50

    signal selectionEdited(int start, int end)

    DayHoursModel
    {
        id: dayHoursModel
        amPmTime: !!locale.amText
    }

    Image
    {
        y: 13
        source: "image://svg/skin/misc/clock_small.svg"
        sourceSize: Qt.size(20, 20)
        visible: !dayHoursModel.amPmTime
    }

    Column
    {
        x: 2
        spacing: 1
        visible: dayHoursModel.amPmTime

        component AmPmLabel: Text
        {
            height: 24
            text: locale.amText
            color: ColorTheme.light
            font.pixelSize: 9
            font.weight: Font.DemiBold
            topPadding: 6
        }
        AmPmLabel { text: locale.amText }
        AmPmLabel { text: locale.pmText }
    }

    Grid
    {
        id: grid

        x: 24
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

                width: grid.cellWidth
                height: 26

                enabled: hour >= minimumHour && hour <= maximumHour

                SelectionMarker
                {
                    anchors
                    {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }

                    start: hour === selectionStart
                    end: hour === selectionEnd
                    visible: hour >= selectionStart && hour <= selectionEnd
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

                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 5
                    font.pixelSize: 11

                    text: model.display

                    color: enabled
                        ? ColorTheme.text
                        : ColorTheme.transparent(ColorTheme.colors.light16, 0.5)
                }

                ArchiveIndicator
                {
                    width: 10
                    cameraHasArchive: periodsVisible && model.hasArchive
                    anyCameraHasArchive: model.anyCameraHasArchive
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
                            selectionStart = hour
                            selectionEnd = hour
                            selectionEdited(hour, hour)
                            return
                        }

                        if (hour < selectionStart)
                        {
                            selectionStart = hour
                            selectionedited(hour, selectionEnd)
                        }
                        else
                        {
                            selectionEnd = hour
                            selectionEdited(selectionStart, hour)
                        }
                    }
                }
            }
        }
    }
}
