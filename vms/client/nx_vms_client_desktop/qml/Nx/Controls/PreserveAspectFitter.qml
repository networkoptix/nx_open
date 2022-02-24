// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

/**
 * A positioner that adjusts its child `contentItem` width and height to scale the dimensions
 * uniformly, preserving contentItem.implicitWidth / contentItem.implicitHeight aspect ratio,
 * to either fit without cropping if `mode` is `PreserveAspectFitter.Fit`,
 * or fill with cropping if necessary if `mode` is `PreserveAspectFitter.Crop`.
 */
Item
{
    id: fitter

    enum Mode { Fit, Crop }

    property Item contentItem
    property int mode: PreserveAspectFitter.Fit

    clip: true

    readonly property real sizeScale:
    {
        if (!contentItem)
            return 1

        if (mode == PreserveAspectFitter.Fit)
        {
            return Math.min(
                width / contentItem.implicitWidth,
                height / contentItem.implicitHeight)
        }

        return Math.max(
            width / contentItem.implicitWidth,
            height / contentItem.implicitHeight)
    }

    onContentItemChanged:
    {
        if (!contentItem)
            return

        contentItem.width = Qt.binding(
            function() { return contentItem.implicitWidth * fitter.sizeScale })

        contentItem.height = Qt.binding(
            function() { return contentItem.implicitHeight * fitter.sizeScale })

        contentItem.parent = fitter
        contentItem.anchors.centerIn = Qt.binding(function() { return contentItem.parent })
    }
}
