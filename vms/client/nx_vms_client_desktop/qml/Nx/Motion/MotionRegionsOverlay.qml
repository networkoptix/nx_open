// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Motion 1.0

import nx.client.core 1.0
import nx.client.desktop 1.0

Item
{
    id: motionRegionsOverlay

    property alias channelIndex: motionRegions.channel
    property alias sensitivityColors: motionRegions.sensitivityColors
    property alias cameraMotionHelper: motionRegions.motionHelper
    property alias rotationQuadrants: motionRegions.rotationQuadrants

    property alias selectionRectVisible: selectionMarker.visible
    property rect selectionRect

    readonly property real kMotionRegionOpacity: 0.3
    readonly property real kSelectionOpacity: 0.2

    readonly property rect selectionMotionRect: correctedSelectionRect === outOfRegionRect
        ? outOfRegionRect
        : itemToMotionRect(correctedSelectionRect)

    function motionToItemRect(motionRect)
    {
        return Qt.rect(
            motionRect.x * motionCellSize.x,
            motionRect.y * motionCellSize.y,
            motionRect.width * motionCellSize.x,
            motionRect.height * motionCellSize.y)
    }

    function itemToMotionRect(itemRect)
    {
        const p1 = itemToMotionPoint(itemRect.left, itemRect.top)
        const p2 = itemToMotionPoint(itemRect.right, itemRect.bottom)
        return Qt.rect(p1.x, p1.y, p2.x - p1.x + 1, p2.y - p1.y + 1)
    }

    function itemToMotionPoint(x, y)
    {
        return Qt.point(
            MathUtils.bound(0, Math.floor(x / motionCellSize.x), MotionDefs.gridWidth - 1),
            MathUtils.bound(0, Math.floor(y / motionCellSize.y), MotionDefs.gridHeight - 1))
    }

    // Private.

    readonly property rect outOfRegionRect: Qt.rect(0, 0, 0, 0)

    readonly property rect correctedSelectionRect:
    {
        const x = MathUtils.bound(0, selectionRect.left, width);
        const y = MathUtils.bound(0, selectionRect.top, height);
        const w = MathUtils.bound(x, selectionRect.right, width) - x
        const h = MathUtils.bound(y, selectionRect.bottom, height) - y

        return (w === 0 || h === 0) ? outOfRegionRect : Qt.rect(x, y, w, h)
    }

    readonly property vector2d motionCellSize:
        Qt.vector2d(width / MotionDefs.gridWidth, height / MotionDefs.gridHeight)

    Rectangle
    {
        id: selectionMarker

        readonly property rect currentRect: motionToItemRect(selectionMotionRect)

        x: currentRect.x
        y: currentRect.y
        width: currentRect.width
        height: currentRect.height

        color: "white"
        opacity: kSelectionOpacity
    }

    MotionRegions
    {
        id: motionRegions
        anchors.fill: parent
        fillOpacity: kMotionRegionOpacity
        borderColor: ColorTheme.window
        labelsColor: ColorTheme.colors.dark1
    }
}
