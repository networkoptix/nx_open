// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Items
import Nx.Items

import nx.vms.client.core

Item
{
    id: root

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
        height:
        {
            if (d.portraitOrientation)
                return heightForWidth(width)

            return root.height
        }
        withNavigationControls: false
        visible: interval.resource
        fullscreenLayout: !d.portraitOrientation

        onShowOnCamera:
        {
            const url = d.notification?.url
            if (url)
                windowContext.uriHandler.handleUrl(url)
        }

        onShowFullscreen: root.fullscreenButtonClicked()
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

        onResourceRemoved: preview.reset()
    }

    QtObject
    {
        id: d

        readonly property bool portraitOrientation: root.width < root.height
        property var notification

        onNotificationChanged:
        {
            notificationDetailsFlickable.contentY = 0

            if (!notification || (notification.url && !notification.url.toString()))
            {
                preview.reset()
                return
            }

            const url = new URL(notification.url)
            const searchParams = new URLSearchParams(url.search);
            const resourceIds = searchParams.get("resources")
            const timestamp = searchParams.get("timestamp")
            if (!resourceIds || !timestamp)
            {
                console.warn("Notification", url, "url does not contain required `resources`` or `timestamp` parameters")
                return
            }

            const ts = Number(timestamp)
            if (!Number.isFinite(ts) || ts < 0)
                return

            const firstResourceId = resourceIds.split(":")[0]
            const camera = windowContext.mainSystemContext.availableCamerasWatcher()
                .getCamera(firstResourceId)

            if (!camera)
            {
                console.log("Resource from the provided url is not exists")
                return;
            }

            preview.interval.resource = camera

            const durationTimeMs = 10000
            const paddingTimeMs = durationTimeMs / 2
            preview.interval.startTimeMs = ts - paddingTimeMs
            preview.interval.durationMs = durationTimeMs + paddingTimeMs * 2
            preview.interval.setPosition(preview.interval.startTimeMs)

            preview.interval.play(preview.interval.startTimeMs) //< Loads the stream anyway.
        }
    }
}
