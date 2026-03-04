// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Mobile.Ui.Sheets
import Nx.Ui

import nx.vms.client.mobile

Action
{
    id: control

    property alias resource: mediaDownloadBackend.resource
    property real positionMs: 0
    property real durationMs: 0
    property alias menuOpened: downloadMediaSheet.opened
    property alias isDownloadAvailable: mediaDownloadBackend.isDownloadAvailable

    enabled: isDownloadAvailable
    text: qsTr("Download")
    icon.source: "image://skin/24x24/Solid/download.svg?primary=light4"
    icon.width: 24
    icon.height: 24

    onTriggered:
    {
        if (durationMs > 0)
            mediaDownloadBackend.downloadVideo(control.positionMs, control.durationMs)
        else
            downloadMediaSheet.open()
    }

    property NxObject d: NxObject
    {
        DownloadMediaDurationSheet
        {
            id: downloadMediaSheet

            onDurationPicked: (duration) =>
            {
                mediaDownloadBackend.downloadVideo(control.positionMs, duration)
            }
        }

        MediaDownloadBackend
        {
            id: mediaDownloadBackend

            onErrorOccured: (title, description) => Workflow.openStandardDialog(title, description)
        }
    }
}
