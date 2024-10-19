// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core

import nx.vms.client.mobile

IconButton
{
    id: control

    property alias resourceId: mediaDownloadBackend.cameraId
    property real positionMs: 0
    property real durationMs: 0
    property alias menuOpened: durationOptionsMenu.opened

    visible: mediaDownloadBackend.isDownloadAvailable
    icon.source: lp("/images/download.svg")
    padding: 0

    onClicked:
    {
        if (durationMs > 0)
        {
            d.downloadVideo(control.durationMs)
            return
        }

        durationOptionsMenu.y = -durationOptionsMenu.height
        durationOptionsMenu.open()
    }

    Menu
    {
        id: durationOptionsMenu

        Repeater
        {
            id: repeater

            model: [
                {"text": qsTr("Download next"), "durationMs": -1},
                d.menuItemForMinutesDownload(20),
                d.menuItemForMinutesDownload(10),
                d.menuItemForMinutesDownload(5),
                d.menuItemForMinutesDownload(3),
                d.menuItemForMinutesDownload(1),
            ]

            delegate: Loader
            {
                sourceComponent: modelData.durationMs > 0
                    ? downloadMenuItem
                    : menuHeaderItem

                Component
                {
                    id: menuHeaderItem

                    MenuHeader
                    {
                        text: modelData.text
                    }
                }

                Component
                {
                    id: downloadMenuItem

                    MenuItem
                    {
                        text: modelData.text
                        enabled: modelData.durationMs != -1
                        visible: true

                        onTriggered:
                        {
                            if (durationMs != -1)
                                d.downloadVideo(modelData.durationMs)

                            durationOptionsMenu.close()
                        }
                    }
                }
            }
        }
    }

    MediaDownloadBackend
    {
        id: mediaDownloadBackend

        onErrorOccured: showMessage(title, description)
    }

    NxObject
    {
        id: d

        function downloadVideo(durationMs)
        {
            mediaDownloadBackend.downloadVideo(control.positionMs, durationMs)
        }

        function menuItemForMinutesDownload(minutesCount)
        {
            const kMsInMinute = 60 * 1000
            return {
                "text": qsTr("%n minutes", "Number of minutes", minutesCount),
                "durationMs": minutesCount * kMsInMinute
            }
        }
    }
}
