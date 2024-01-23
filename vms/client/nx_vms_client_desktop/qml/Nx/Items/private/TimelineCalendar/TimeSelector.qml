// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

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
    property alias timeZone: dayHoursModel.timeZone
    property alias periodStorage: dayHoursModel.periodStorage
    property alias allCamerasPeriodStorage: dayHoursModel.allCamerasPeriodStorage
    property bool periodsVisible: true

    implicitHeight: grid.implicitHeight

    signal selectionEdited(int start, int end)

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
            color: enabled ? ColorTheme.buttonText : ColorTheme.colors.light16
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

                width: grid.cellWidth
                height: 32

                enabled: hourItem.hour >= minimumHour && hourItem.hour <= maximumHour
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

                    start: hour === selectionStart
                    end: hour === selectionEnd

                    visible: enabled && hour >= selectionStart && hour <= selectionEnd
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
                    color: enabled ? ColorTheme.buttonText : ColorTheme.colors.light16
                }

                ArchiveIndicator
                {
                    width: 12
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
                            selectionStart = hour
                            selectionEnd = hour
                            selectionEdited(hour, hour)
                            return
                        }

                        if (hour < selectionStart)
                        {
                            selectionStart = hour
                            selectionEdited(hour, selectionEnd)
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
