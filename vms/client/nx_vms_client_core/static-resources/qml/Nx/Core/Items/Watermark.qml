// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core

Image
{
    id: control

    property alias resource: contextAccessor.resource
    readonly property var watermarkId: NxGlobals.generateUuid()
    property size sourceSize
    readonly property string text: d.systemContext?.watermarkWatcher.username ?? ""

    visible: !!source.toString()
    source: d.systemContext
        ? d.systemContext.watermarkWatcher.watermarkImageUrl(watermarkId)
        : ""

    onSourceSizeChanged:
    {
        if (d.systemContext)
        {
            d.systemContext.watermarkWatcher.updateWatermarkImageUrlSize(
                control.watermarkId, control.sourceSize)
        }
    }

    SystemContextAccessor
    {
        id: contextAccessor

        onSystemContextChanged:
        {
            systemContext.watermarkWatcher.addWatermarkImageUrlWatcher(
                control.watermarkId, sourceSize)
        }

        onSystemContextIsAboutToBeChanged:
        {
            d.removeWatcher()
        }
    }

    Component.onDestruction:
    {
        d.removeWatcher()
    }

    NxObject
    {
        id: d

        readonly property alias systemContext: contextAccessor.systemContext

        function removeWatcher()
        {
            if (systemContext)
                systemContext.watermarkWatcher.removeWatermarkImageUrlWatcher(control.watermarkId)
        }
    }

    Connections
    {
        target: d.systemContext
            ? d.systemContext.watermarkWatcher
            : null

        function onWatermarkImageUrlChanged(id)
        {
            if (d.systemContext && id === control.watermarkId)
                control.source = d.systemContext.watermarkWatcher.watermarkImageUrl(watermarkId)
        }
    }
}
