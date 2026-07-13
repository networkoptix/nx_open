// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Items
import Nx.Items
import Nx.Ui

import nx.vms.client.core

Item
{
    id: root

    readonly property string title: resourceHelper.resourceName || qsTr("Details")
    readonly property bool hasPreview: d.currentCamera !== null
    property alias notification: d.notification

    signal fullscreenButtonClicked

    Preview
    {
        id: preview

        function reset()
        {
            interval.resource = null
            interval.startTimeMs = 0
            interval.durationMs = 0
            interval.setPosition(0)
        }

        width: parent.width
        height: d.portraitOrientation ? root.height * 0.35 : root.height

        withNavigationControls: d.resourceCount > 1
        hasPrevious: d.currentIndex > 0
        hasNext: d.currentIndex < d.resourceCount - 1

        visible: interval.resource
        fullscreenLayout: !d.portraitOrientation

        onShowOnCamera: d.showOnCamera()

        onShowFullscreen: root.fullscreenButtonClicked()
        onPrevious: d.selectResource(d.currentIndex - 1)
        onNext: d.selectResource(d.currentIndex + 1)
    }

    Flickable
    {
        id: notificationDetailsFlickable

        anchors.fill: parent
        anchors.margins: 20
        anchors.topMargin: 20 + (preview.visible ? preview.height : 0)

        contentHeight: notificationDetailsColumn.implicitHeight
        clip: true

        ColumnLayout
        {
            id: notificationDetailsColumn

            width: parent.width
            spacing: 16

            Text
            {
                Layout.fillWidth: true

                text: root.notification?.title ?? ""
                visible: !!text
                font.pixelSize: 18
                font.weight: Font.Medium
                color: ColorTheme.colors.light4
                wrapMode: Text.WordWrap
            }

            Text
            {
                Layout.fillWidth: true

                text: root.notification?.description ?? ""
                visible: !!text
                font.pixelSize: 16
                color: ColorTheme.colors.light10
                wrapMode: Text.WordWrap
            }

            Text
            {
                Layout.fillWidth: true

                font.pixelSize: 14
                font.capitalization: Font.AllUppercase
                font.weight: Font.Medium

                color: ColorTheme.colors.light16
                text: root.notification?.time ?? ""
            }
        }
    }

    ResourceHelper
    {
        id: resourceHelper

        resource: preview.interval.resource

        onResourceRemoved: d.reset()
    }

    // Loads recorded time periods for the current camera so we can tell whether the archive covers
    // the requested timestamp before touching the media player.
    ChunkProvider
    {
        id: archiveProvider

        onLoadingChanged: d.evaluateArchive()
        onPeriodsUpdated: (contentType) =>
        {
            if (contentType === ChunkProvider.RecordingContent)
                d.evaluateArchive()
        }
    }

    QtObject
    {
        id: d

        readonly property bool portraitOrientation: root.width < root.height
        property var notification

        // Ids of the resolvable resources from the notification url, in their original order.
        property var resourceIds: []
        property int currentIndex: 0
        property real timestamp: -1
        property var currentCamera: null

        readonly property int resourceCount: resourceIds.length

        function reset()
        {
            preview.reset()
            archiveProvider.resource = null
            resourceIds = []
            currentIndex = 0
            timestamp = -1
            currentCamera = null
            preview.dataState = Preview.DataState.Checking
        }

        function startPlayback()
        {
            const durationTimeMs = 10000
            const paddingTimeMs = durationTimeMs / 2
            preview.interval.startTimeMs = timestamp - paddingTimeMs
            preview.interval.durationMs = durationTimeMs + paddingTimeMs * 2
            preview.interval.setPosition(preview.interval.startTimeMs)
            preview.interval.play(preview.interval.startTimeMs) //< Loads the stream.
        }

        function evaluateArchive()
        {
            if (!currentCamera || timestamp < 0)
                return

            if (archiveProvider.hasArchive(timestamp))
            {
                if (preview.dataState !== Preview.DataState.Available)
                {
                    preview.dataState = Preview.DataState.Available
                    startPlayback()
                }
            }
            else if (!archiveProvider.loading)
            {
                preview.dataState = Preview.DataState.NoData
            }
            else
            {
                preview.dataState = Preview.DataState.Checking
            }
        }

        function selectResource(index)
        {
            if (index < 0 || index >= resourceIds.length)
                return

            currentIndex = index
            currentCamera = windowContext.mainSystemContext.availableCamerasWatcher()
                .getCamera(resourceIds[index])

            preview.reset()
            preview.dataState = Preview.DataState.Checking

            if (!currentCamera)
            {
                archiveProvider.resource = null
                return
            }

            preview.interval.resource = currentCamera //< Makes the preview visible, no stream yet.
            archiveProvider.resource = currentCamera //< Triggers async archive periods loading.
            evaluateArchive() //< Handle the case when the periods are already cached.
        }

        function showOnCamera()
        {
            Workflow.openVideoScreen(currentCamera, timestamp)
        }

        onNotificationChanged:
        {
            notificationDetailsFlickable.contentY = 0
            reset()

            if (!notification || (notification.url && !notification.url.toString()))
                return

            const url = new URL(notification.url)
            const searchParams = new URLSearchParams(url.search);
            const resources = searchParams.get("resources")
            const ts = searchParams.get("timestamp")
            if (!resources || !ts)
            {
                console.warn("Notification", url, "url does not contain required `resources` or `timestamp` parameters")
                return
            }

            const tsNumber = Number(ts)
            if (!Number.isFinite(tsNumber) || tsNumber < 0)
                return

            const watcher = windowContext.mainSystemContext.availableCamerasWatcher()
            const availableIds = resources.split(":").filter(id => !!watcher.getCamera(id))
            if (availableIds.length === 0)
            {
                console.log("Resources from the provided url do not exist")
                return
            }

            timestamp = tsNumber
            resourceIds = availableIds
            selectResource(0)
        }
    }
}
