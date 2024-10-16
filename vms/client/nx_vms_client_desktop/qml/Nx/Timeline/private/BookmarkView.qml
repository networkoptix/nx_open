// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

import nx.vms.client.core

Control
{
    id: control

    property var bookmark

    signal tagClicked(string tag)

    padding: 16

    contentItem: ColumnLayout
    {
        spacing: 12

        Text
        {
            Layout.fillWidth: true

            text: control.bookmark.name
            font.pixelSize: FontConfig.large.pixelSize
            font.weight: FontConfig.large.weight
            color: "white"
            wrapMode: Text.Wrap
        }

        Text
        {
            text: "%1 | %2"
               .arg(NxGlobals.timeInShortFormat(control.bookmark.dateTime))
               .arg(NxGlobals.dateInShortFormat(control.bookmark.dateTime))

            color: "white"
            font.pixelSize: FontConfig.small.pixelSize
            font.weight: FontConfig.small.weight
        }

        Flow
        {
            spacing: 4
            visible: control.bookmark.tags.length

            Repeater
            {
                model: control.bookmark.tags

                Tag
                {
                    text: modelData
                    textColor: "#51ACF6"
                    textCapitalization: Font.MixedCase
                    color: "transparent"
                    border.color: textColor
                    border.width: 1

                    MouseArea
                    {
                        anchors.fill: parent
                        onClicked:
                        {
                            control.tagClicked(modelData)
                        }
                    }
                }
            }
        }

        Text
        {
            Layout.fillWidth: true

            text: control.bookmark.description
            color: "white"
            wrapMode: Text.Wrap
            visible: text
        }
    }
}
