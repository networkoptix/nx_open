// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Models 1.0
import Nx.Controls 1.0 as Nx
import nx.vms.client.desktop 1.0

import "tile"

Item
{
    id: tileGrid

    property int maxVisibleRowCount: 0

    property Item openedTileParent: null
    property var model: null

    property bool alignToCenter: true
    property bool hideActionEnabled: true
    property bool shiftPressed: false

    readonly property int tileSpacing: 16
    readonly property int minTileWidth: 260
    readonly property int maxTileWidth: 360

    readonly property int maxColCount: (width + tileSpacing) / (minTileWidth + tileSpacing)
    readonly property int colCount: model ? MathUtils.bound(1, model.systemsCount, maxColCount) : 0

    readonly property int visibleRowCount: model
        ? MathUtils.bound(1, Math.ceil(model.systemsCount / colCount), maxVisibleRowCount)
        : 0

    readonly property int _intermediateColCount: alignToCenter ? colCount : maxColCount
    readonly property int _intermediateTileWidth:
        (width - tileSpacing * (_intermediateColCount - 1)) / _intermediateColCount
    readonly property int tileWidth: Math.min(_intermediateTileWidth, maxTileWidth)
    readonly property int tileHeight: 100

    readonly property alias firstRowYShift: grid.y

    readonly property alias scrollBarVisible: scrollBar.visible

    signal tileClicked()
    signal lockInterface(bool locked)
    signal setOpenedTileModalityInterfaceLock(bool enabled)

    function closeOpenedTile()
    {
        for (let item of systemTilesStorage.items)
        {
            if (item.isExpanded)
            {
                item.shrinkAndStopConnecting()
                return
            }
        }
    }

    Item
    {
        anchors.fill: parent

        clip: true

        GridView
        {
            id: grid

            x: (tileGrid.width - width + tileGrid.tileSpacing) / 2
            y: (tileGrid.height - height + tileGrid.tileSpacing) / 2
            width: (tileGrid.alignToCenter ? tileGrid.colCount : tileGrid.maxColCount) *
                cellWidth
            height: (tileGrid.alignToCenter
                ? tileGrid.visibleRowCount
                : tileGrid.maxVisibleRowCount)
                    * cellHeight

            cellWidth: tileGrid.tileWidth + tileGrid.tileSpacing
            cellHeight: tileGrid.tileHeight + tileGrid.tileSpacing

            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            snapMode: GridView.SnapToRow
            ScrollBar.vertical: scrollBar

            model: tileGrid.model

            delegate: Loader
            {
                readonly property bool tileIsVisible: (y >= grid.contentY)
                    && (y + grid.cellHeight <= grid.contentY + grid.height)

                onTileIsVisibleChanged:
                {
                    if (!tileIsVisible)
                        item.closeTileMenu()
                }

                sourceComponent:
                {
                    if (model.tileType === ConnectTilesModel.CloudButtonTileType)
                        return cloudTile
                    if (model.tileType === ConnectTilesModel.ConnectButtonTileType)
                        return connectTile
                    return normalTile
                }

                Component
                {
                    id: normalTile

                    Tile
                    {
                        openedParent: tileGrid.openedTileParent

                        gridCellWidth: tileGrid.tileWidth
                        gridCellHeight: tileGrid.tileHeight

                        onVisibilityScopeChanged:
                        {
                            if (model.visibilityScope !== visibilityScope)
                                model.visibilityScope = visibilityScope
                        }

                        onStartExpanding:
                        {
                            for (let item of systemTilesStorage.items)
                            {
                                if (item.systemId !== systemId && item.isExpanded)
                                {
                                    item.shrink()
                                    return
                                }
                            }
                            tileGrid.setOpenedTileModalityInterfaceLock(true)
                        }

                        onShrinked: tileGrid.setOpenedTileModalityInterfaceLock(false)

                        visibilityMenuModel: model
                        hideActionEnabled: tileGrid.hideActionEnabled
                        shiftPressed: tileGrid.shiftPressed

                        Component.onCompleted: systemTilesStorage.addItem(this);
                        Component.onDestruction: systemTilesStorage.removeItem(this);
                        onClickedToConnect:
                        {
                            tileGrid.lockInterface(true)
                            tileGrid.tileClicked()
                        }
                        onReleaseFocus: tileGrid.focus = true
                    }
                }

                Component
                {
                    id: cloudTile

                    CloudTile
                    {
                        width: tileGrid.tileWidth
                        height: tileGrid.tileHeight

                        visibilityMenuModel: model
                        hideActionEnabled: tileGrid.hideActionEnabled
                    }
                }

                Component
                {
                    id: connectTile

                    ConnectTile
                    {
                        width: tileGrid.tileWidth
                        height: tileGrid.tileHeight
                    }
                }
            }

            QtObject
            {
                id: systemTilesStorage

                property variant items: []

                function addItem(item)
                {
                    console.assert(item, "Unexpected null tile");
                    if (items.indexOf(item) === -1)
                        items.push(item)
                }

                function removeItem(item)
                {
                    const index = items.indexOf(item)
                    if (index > -1)
                        items.splice(index, 1) //< Removes element.
                }
            }

            Connections
            {
                // Handles outer signal that expands tile.
                id: openTileHandler

                target: context

                function findTile(systemId)
                {
                    if (systemId.length !== 0)
                    {
                        for (let item of systemTilesStorage.items)
                        {
                            if (item.systemId === systemId)
                                return item
                        }
                    }

                    return null
                }

                function onOpenTile(systemId, errorMessage, isLoginError)
                {
                    tileGrid.lockInterface(false)

                    if (systemId.length !== 0)
                    {
                        let item = findTile(systemId)
                        if (item)
                        {
                            const isConnecting = item.isConnecting
                            item.setErrorMessage(errorMessage, isLoginError)

                            if (isConnecting
                                && !item.cloud
                                && !item.isFactorySystem
                                && !item.isExpanded
                                && !item.incompatible
                                && item.online)
                            {
                                item.expand()
                            }
                        }
                    }
                    else
                    {
                        const tilesNumber = systemTilesStorage.items.length
                        for (let item of systemTilesStorage.items)
                            item.setErrorMessage("", isLoginError)
                    }
                }

                function onCloseTile(systemId)
                {
                    let item = findTile(systemId)
                    if (item && item.isExpanded)
                        item.shrinkAndStopConnecting()
                }
            }

            MouseArea
            {
                anchors.fill: parent

                onPressed: mouse =>
                {
                    scrollBar.focus = true
                    mouse.accepted = false
                }
            }
        }
    }

    Nx.ScrollBar
    {
        id: scrollBar

        readonly property int fullRowNumber: model
            ? Math.max(Math.ceil(model.systemsCount / tileGrid.colCount), tileGrid.visibleRowCount)
            : 0

        x: tileGrid.width + 8
        height: grid.height - tileGrid.tileSpacing

        policy: ScrollBar.AlwaysOn
        snapMode: ScrollBar.SnapAlways

        visible: fullRowNumber != tileGrid.visibleRowCount

        stepSize: 1 / (Math.max(fullRowNumber - tileGrid.visibleRowCount, 1))
    }
}
