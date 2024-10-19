// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Mobile

GridView
{
    id: camerasGrid

    property real spacing: 8
    property alias layoutId: camerasModel.layoutId
    property bool keepStatuses: false
    property bool active: false
    property alias filterIds: camerasModel.filterIds

    cellWidth: (width - leftMargin - rightMargin) / d.columnsCount
    cellHeight: cellWidth * 9 / 16 + 24 + 16

    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    NxObject
    {
        id: d

        readonly property real maxItemWidth: 320 + camerasGrid.spacing
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

        readonly property int columnsCount:
        {
            const columns = Math.ceil(camerasGrid.width / maxItemWidth)
            return Math.max(2, Math.min(columns, maxColumnsCount))
        }
    }

    model: QnCameraListModel
    {
        id: camerasModel
    }

    delegate: CameraItem
    {
        width: cellWidth
        height: cellHeight

        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail
        keepStatus: camerasGrid.keepStatuses
        resource: model.resource
        active: camerasGrid.active

        onClicked: camerasGrid.openVideoScreen(model.resource, model.thumbnail, camerasModel)

        onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)
    }
}
