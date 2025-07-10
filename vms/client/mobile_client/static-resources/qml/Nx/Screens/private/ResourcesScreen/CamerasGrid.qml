// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Mobile

GridView
{
    id: control

    property alias layout: camerasModel.layout
    property bool keepStatuses: false
    property bool active: false
    property alias filterIds: camerasModel.filterIds
    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    property real spacing: 2

    cellWidth: d.cellWidth
    cellHeight: d.cellHeight

    model: QnCameraListModel
    {
        id: camerasModel
    }

    delegate: Item
    {
        property alias mediaPlayer: cameraItem.mediaPlayer

        width: d.cellWidth
        height: d.cellHeight

        CameraItem
        {
            id: cameraItem

            aspectRatio: d.kAspectRatio
            anchors.fill: parent
            anchors.margins: control.spacing / 2

            resourceName: model.resourceName
            status: model.resourceStatus
            thumbnail: model.thumbnail
            keepStatus: control.keepStatuses
            resource: model.resource
            active: control.active

            onClicked: control.openVideoScreen(model.resource, model.thumbnail, camerasModel)

            onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)
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
        readonly property real cellWidth: Math.floor(d.availableWidth / d.columnsCount)
        readonly property real cellHeight:
            (cellWidth - control.spacing / 2) * kAspectRatio + control.spacing / 2

        function getColumnsCount(itemWidth)
        {
            const columns = Math.ceil(d.availableWidth / itemWidth)
            return Math.max(2, Math.min(columns, d.maxColumnsCount))
        }
    }
}
