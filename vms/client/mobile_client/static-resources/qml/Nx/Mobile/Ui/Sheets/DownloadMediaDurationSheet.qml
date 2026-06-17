// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Common
import Nx.Core
import Nx.Mobile
import Nx.Mobile.Controls

AdaptiveSheet
{
    id: sheet

    signal durationPicked(int duration)

    titleTextItem
    {
        text: qsTr("Download next")

        horizontalAlignment: Text.AlignHCenter

        font.pixelSize: 16
        font.weight: 400
        color: ColorTheme.colors.light18
    }

    spacing: 0
    contentSpacing: 0

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

            Rectangle
            {
                visible: index < repeater.count
                height: 1
                width: parent.width
                anchors.top: parent.top
                color: ColorTheme.colors.dark12
            }

            Text
            {
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter

                font.pixelSize: 16
                font.weight: 400
                color: ColorTheme.colors.light10

                text: modelData.text
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
        text: qsTr("Cancel")
        type: Button.LightInterface

        onClicked: sheet.close()
    }
}
