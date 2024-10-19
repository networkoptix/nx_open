// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Nx.Core
import Nx.Mobile

ListView
{
    id: camerasList

    spacing: 8

    model: QnCameraListModel {}

    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    delegate: CameraListItem
    {
        width: camerasList.width
        text: model.resourceName
        status: model.resourceStatus
        thumbnail: model.thumbnail

        onClicked: openVideoScreen(model.resource, model.thumbnail, camerasList.model)
    }
}
