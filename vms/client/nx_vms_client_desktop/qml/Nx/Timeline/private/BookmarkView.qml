// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import Nx.Controls

import "."

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

        LimitedFlow
        {
            id: tagFlow

            Layout.fillWidth: true

            visible: control.bookmark.tags.length
            spacing: 4
            rowLimit: 1

            delegate: Tag
            {
                text: model.tag
                onClicked:
                {
                    control.tagClicked(model.tag)
                }
            }

            sourceModel: ListModel
            {
                id: tagListModel
            }

            Tag
            {
                text: "+" + tagFlow.remaining
                onClicked:
                {
                    tagFlow.rowLimit *= 3
                }
            }

            Component.onCompleted:
            {
                for (let tag of control.bookmark.tags)
                    tagListModel.append({"tag": tag})
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
