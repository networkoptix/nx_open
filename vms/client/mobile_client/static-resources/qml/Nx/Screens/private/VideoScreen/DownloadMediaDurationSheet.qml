// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Mobile
import Nx.Mobile.Controls

BaseAdaptiveSheet
{
    id: sheet

    signal durationPicked(int duration)

    Repeater
    {
        id: repeater

        function menuItemForMinutesDownload(minutesCount)
        {
            const kMsInMinute = 60 * 1000
            return {
                "text": qsTr("%n minutes", "Number of minutes", minutesCount),
                "durationMs": minutesCount * kMsInMinute
            }
        }

        model: [
            {"text": qsTr("Download next"), "durationMs": -1},
            menuItemForMinutesDownload(20),
            menuItemForMinutesDownload(10),
            menuItemForMinutesDownload(5),
            menuItemForMinutesDownload(3),
            menuItemForMinutesDownload(1),
        ]

        delegate: Item
        {
            width: parent.width
            height: 52

            Text
            {
                anchors.fill: parent
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                font.pixelSize: 16
                font.weight: 400
                color: ColorTheme.colors.light10

                text: modelData.text
            }

            Rectangle
            {
                visible: index < repeater.count - 1
                height: 1
                width: parent.width
                anchors.bottom: parent.bottom
                color: ColorTheme.colors.dark12
            }

            MouseArea
            {
                anchors.fill: parent
                onClicked:
                {
                    durationPicked(modelData.durationMs)
                    sheet.close()
                }
            }
        }
    }

    Item
    {
        width: parent.width
        height: 10
    }

    footer: Button
    {
        text: qsTr("Close")
        type: Button.LightInterface

        onClicked: sheet.close()
    }
}
