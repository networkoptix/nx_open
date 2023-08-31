// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx 1.0
import Nx.Core 1.0

import ".."

Control
{
    id: tile

    property TileController controller: null

    readonly property bool selected: controller && controller.selectedRow === index
    onSelectedChanged:
    {
        if (selected)
            controller.selectedTile = tile
    }

    function getData(name)
    {
        return model[name]
    }

    hoverEnabled: true

    property color backgroundColor: model.isInformer
        ? ColorTheme.colors.dark8
        : ColorTheme.colors.dark7

    property color hoveredBackgroundColor: model.isInformer
        ? ColorTheme.colors.dark9
        : ColorTheme.colors.dark8

    property color effectiveBackgroundColor:
    {
        let highlightAmount = model.highlighted ? 2 : 0
        if (controller && controller.selectedRow === index)
            highlightAmount += 6

        return ColorTheme.lighter(
            tile.hovered ? tile.hoveredBackgroundColor : tile.backgroundColor,
            highlightAmount)
    }

    property color foregroundColor: ColorTheme.colors.light10
    property color secondaryForegroundColor: ColorTheme.colors.light16

    leftPadding: 8
    topPadding: 8
    rightPadding: 8
    bottomPadding: 8

    background: Rectangle
    {
        color: tile.effectiveBackgroundColor
        radius: 2

        TapHandler
        {
            enabled: !!tile.controller

            onSingleTapped:
                tileController.clicked(index, Qt.LeftButton, point.modifiers)

            onDoubleTapped:
                tileController.doubleClicked(index)
        }

        TapHandler
        {
            acceptedButtons: Qt.MiddleButton
            enabled: !!tile.controller

            onSingleTapped:
                tileController.clicked(index, Qt.MiddleButton, point.modifiers)
        }

        TapHandler
        {
            acceptedButtons: Qt.RightButton
            enabled: !!tile.controller

            onSingleTapped:
            {
                tileController.contextMenuRequested(
                    index, parent.mapToGlobal(point.position.x, point.position.y))
            }
        }

        DragHandler
        {
            target: null
            enabled: !!tile.controller

            onActiveChanged:
            {
                if (!active)
                    return

                tileController.dragStarted(
                    index, centroid.pressPosition, Qt.size(tile.width, tile.height))
            }
        }
    }

    onHoveredChanged:
    {
        if (tile.controller)
            tile.controller.hoverChanged(index, tile.hovered)
    }
}
