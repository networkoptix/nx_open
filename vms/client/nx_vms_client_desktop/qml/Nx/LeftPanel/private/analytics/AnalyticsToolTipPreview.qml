// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: preview

    property int previewState
    property real previewAspectRatio: 1

    property alias videoPreviewResourceId: intervalPreview.resourceId
    property alias videoPreviewTimestampMs: intervalPreview.timestampMs

    padding: rectangle.border.width

    background: Rectangle
    {
        id: rectangle

        border.color: ColorTheme.darker(ColorTheme.midlight, 2)
        color: ColorTheme.darker(ColorTheme.midlight, 1)
    }

    contentItem: Item
    {
        id: content

        // Preview item should not have aspect ratio lesser than this value, by design.
        // Actual preview image is fit into these bounds keeping its own aspect ratio.
        readonly property real kItemMinimumAspectRatio: 16.0 / 9.0

        implicitHeight: width / Math.max(previewAspectRatio, kItemMinimumAspectRatio)

        IntervalPreview
        {
            id: intervalPreview

            loopDelayMs: CoreSettings.iniConfigValue("intervalPreviewLoopDelayMs")
            durationMs: CoreSettings.iniConfigValue("intervalPreviewDurationMs")
            speedFactor: CoreSettings.iniConfigValue("intervalPreviewSpeedFactor")

            anchors.fill: parent
            active: true
            aspectRatio: preview.previewAspectRatio

            // Some delay to avoid opening many RTSP streams while moving the mouse pointer over
            // several analytics tiles.
            delayMs: 200
        }

        Text
        {
            id: noData

            anchors.fill: parent
            verticalAlignment: Qt.AlignVCenter
            horizontalAlignment: Qt.AlignHCenter
            visible: intervalPreview.previewState === RightPanel.PreviewState.missing
            color: ColorTheme.windowText
            text: qsTr("NO DATA")
        }

        NxDotPreloader
        {
            id: preloader

            anchors.centerIn: parent
            color: ColorTheme.windowText

            running: intervalPreview.previewState === RightPanel.PreviewState.busy
                || intervalPreview.previewState === RightPanel.PreviewState.initial
        }
    }
}
