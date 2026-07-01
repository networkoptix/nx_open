// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Mobile
import Nx.Models
import Nx.Screens

import ".."

Item
{
    id: switcher

    required property Item videoItem
    required property QnCameraListModel camerasModel
    required property VideoScreenController controller

    property alias interactive: listView.interactive
    property alias spacing: listView.spacing
    property color backgroundColor: ColorTheme.colors.dark6

    readonly property bool switching: listView.moving

    readonly property real transitionFraction: Math.abs(d.swipeOffset / switcher.width)

    readonly property string resourceName: centeredResourceHelper.qualifiedResourceName
    readonly property alias resource: d.centeredResource
    readonly property string lastThumbnail: d.thumbnail

    clip: true

    function switchToPreviousCamera()
    {
        if (listView.count > 1 && !listView.moving)
            listView.currentIndex = (listView.currentIndex + listView.count - 1) % listView.count
    }

    function switchToNextCamera()
    {
        if (listView.count > 1 && !listView.moving)
            listView.currentIndex = (listView.currentIndex + 1) % listView.count
    }

    ListView
    {
        id: listView

        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true

        anchors.fill: parent
        orientation: Qt.Horizontal
        snapMode: ListView.SnapOneItem
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 4 * width
        cacheBuffer: width
        reuseItems: true
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: 0
        preferredHighlightEnd: 0
        highlightMoveDuration: 250

        model: switcher.camerasModel

        delegate: Rectangle
        {
            id: delegateItem

            property bool pooled: false
            readonly property var modelData: pooled ? undefined : model

            readonly property var resource: modelData?.resource ?? null
            readonly property var thumbnail: modelData?.thumbnail ?? ""

            property bool controlled: delegateItem.resource === switcher.controller?.resource
            property bool screenshotSuppressed: false

            width: switcher.width
            height: switcher.height

            color: switcher.backgroundColor

            Image
            {
                id: screenshot

                anchors.fill: delegateItem
                visible: !delegateItem.screenshotSuppressed

                source: delegateItem.thumbnail ?? ""
                fillMode: Image.PreserveAspectFit
            }

            VideoDummy
            {
                id: overlay

                anchors.fill: delegateItem
                state: overlayController.dummyState
                visible: !delegateItem.screenshotSuppressed && !!state
                interactive: false

                SimpleVideoOverlayController
                {
                    id: overlayController

                    mediaResourceHelper: MediaResourceHelper
                    {
                        resource: delegateItem.resource
                    }
                }
            }

            LayoutItemProxy
            {
                id: videoItemProxy

                anchors.fill: delegateItem
                target: delegateItem.controlled ? switcher.videoItem : null
            }

            ListView.onPooled:
                pooled = true

            ListView.onReused:
            {
                screenshotSuppressed = false
                pooled = false
            }

            onControlledChanged:
            {
                if (!controlled)
                    screenshotSuppressed = false
            }

            Connections
            {
                target: switcher.controller?.mediaPlayer
                enabled: delegateItem.controlled

                function onPlayingChanged()
                {
                    if (switcher.controller.mediaPlayer.playing)
                        delegateItem.screenshotSuppressed = true
                }
            }
        }

        onMovementEnded:
        {
            if (controller.resource === d.centeredResource)
                return

            controller.setResource(d.centeredResource)
            d.thumbnail = listView.currentItem?.thumbnail ?? ""
        }
    }

    MediaResourceHelper
    {
         id: centeredResourceHelper
         resource: d.centeredResource
    }

    NxObject
    {
        id: d

        readonly property var centeredResource: listView.currentItem?.resource ?? null
        property string thumbnail: ""

        readonly property real swipeOffset: listView.currentItem
            ? (listView.currentItem.x - listView.contentX)
            : 0

        readonly property var resource: controller?.resource ?? null

        function syncToResource()
        {
            const row = camerasModel.rowByResource(d.resource)
            if (row != -1)
                listView.positionViewAtIndex(row, ListView.Beginning)

            listView.currentIndex = row
        }

        onResourceChanged:
            d.syncToResource()

        Component.onCompleted:
            d.syncToResource()
    }
}
