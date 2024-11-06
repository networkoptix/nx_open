// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Image
{
    id: control

    readonly property var watermarkId: NxGlobals.generateUuid()
    property size sourceSize

    visible: !!source
    source: systemContext.watermarkWatcher.watermarkSourceUrl(watermarkId)

    Component.onCompleted:
        systemContext.watermarkWatcher.addWatermarkImageUrlWatcher(control.watermarkId, sourceSize)
    Component.onDestruction:
        systemContext.watermarkWatcher.removeWatermarkImageUrlWatcher(control.watermarkId)
    onSourceSizeChanged:
        systemContext.watermarkWatcher.updateWatermarkImageUrlSize(control.watermarkId, control.sourceSize)

    Connections
    {
        target: systemContext.watermarkWatcher

        function onWatermarkImageUrlChanged(id)
        {
            if (id === control.watermarkId)
                control.source = systemContext.watermarkWatcher.watermarkImageUrl(watermarkId)
        }
    }
}
