// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Items
import Nx.Core.Controls
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core

MouseArea
{
    id: control

    property bool isAnalyticsItem: true
    property QnCameraListModel camerasModel: null
    property EventSearchModel eventsModel: null
    property int currentEventIndex: -1
    property alias previewId: preview.previewId
    property alias previewState: preview.previewState
    property var resource
    property var timestampMs
    property alias title: titleItem.text
    property alias extraText: extraTextItem.text
    property alias eventTimestampText: eventTimeItem.text

    height: d.kRowHeight
    width: (parent && parent.width) ?? 0

    onClicked: Workflow.openEventDetailsScreen(
        camerasModel, eventsModel, currentEventIndex, isAnalyticsItem)

    Preview
    {
        id: preview

        x: 12
        width: parent.height * d.kPreviewAspect
        height: parent.height
    }

    Item
    {
        id: infoHolder

        height: parent.height
        x: preview.x + preview.width + 8
        width: parent.width - x - 15

        Column
        {
            width: parent.width
            spacing: 2

            Text
            {
                id: titleItem

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light4
                font.pixelSize: 16
                font.weight: Font.Medium
            }

            Text
            {
                id: extraTextItem

                width: parent.width
                visible: !!plainText(text)
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light10
                font.pixelSize: 12
                font.weight: Font.Normal
            }

            Text
            {
                id: cameraName

                width: parent.width
                visible: !!text
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 1

                color: ColorTheme.colors.dark13
                font.pixelSize: 10
                font.weight: Font.Medium

                text: control.resource.name
            }

            Text
            {
                id: eventTimeItem

                width: parent.width
                visible: text.length
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 1

                color: ColorTheme.colors.dark14
                font.pixelSize: 10
                font.weight: Font.Medium

                text:
                {
                    return EventSearchUtils.timestampText(control.timestampMs)
                }
            }
        }
    }

    NxObject
    {
        id: d

        readonly property real kPreviewAspect: 16/9
        readonly property int kRowHeight: 100
    }
}
