// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Mobile

Flickable
{
    id: control

    property alias layout: camerasModel.layout
    property bool keepStatuses: false
    property bool active: false
    property alias filterIds: camerasModel.filterIds
    property alias count: repeater.count
    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    contentWidth: width
    contentHeight: flowLayout.height

    function itemAtIndex(index)
    {
        return repeater.itemAt(index)
    }

    Flow
    {
        id: flowLayout

        spacing: 2
        width: d.availableWidth

        Repeater
        {
            id: repeater

            model: QnCameraListModel
            {
                id: camerasModel
            }

            CameraItem
            {
                aspectRatio: d.kAspectRatio
                width: d.cellWidth
                height: d.cellHeight

                text: model.resourceName
                status: model.resourceStatus
                thumbnail: model.thumbnail
                keepStatus: control.keepStatuses
                resource: model.resource
                active: control.active

                onClicked: control.openVideoScreen(model.resource, model.thumbnail, camerasModel)

                onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)
            }
        }
    }

    NxObject
    {
        id: d

        readonly property real kAspectRatio: 9 / 16
        readonly property real availableWidth: control.width - leftMargin - rightMargin
        property int maxColumnsCount:
        {
            switch (Screen.primaryOrientation)
            {
                case Qt.LandscapeOrientation:
                case Qt.InvertedLandscapeOrientation:
                    return 4
                default:
                    return 3
            }
        }

        readonly property int columnsCount: d.getColumnsCount(/**/320)
        readonly property real cellWidth: Math.floor(
            (d.availableWidth - (d.columnsCount - 1) * flowLayout.spacing) / d.columnsCount)
        readonly property real cellHeight: cellWidth * kAspectRatio

        function getColumnsCount(itemWidth)
        {
            const spacing = flowLayout.spacing
            const columns = Math.ceil((d.availableWidth - spacing) / (spacing + itemWidth))
            return Math.max(2, Math.min(columns, d.maxColumnsCount))
        }
    }
}
