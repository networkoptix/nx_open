// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Image
{
    id: control

    readonly property var watermarkId: NxGlobals.generateUuid()
    property size sourceSize

    visible: !!source
    source: watermarkWatcher.watermarkSourceUrl(watermarkId)

    Component.onCompleted:
        watermarkWatcher.addWatermarkImageUrlWatcher(control.watermarkId, sourceSize)
    Component.onDestruction:
        watermarkWatcher.removeWatermarkImageUrlWatcher(control.watermarkId)
    onSourceSizeChanged:
        watermarkWatcher.updateWatermarkImageUrlSize(control.watermarkId, control.sourceSize)

    Connections
    {
        target: watermarkWatcher

        function onWatermarkImageUrlChanged(id)
        {
            if (id === control.watermarkId)
                control.source = watermarkWatcher.watermarkImageUrl(watermarkId)
        }
    }
}
