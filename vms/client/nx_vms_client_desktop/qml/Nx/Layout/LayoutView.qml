// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Nx
import Nx.Core
import Nx.Items
import Nx.Utils

import nx.vms.client.core
import nx.vms.client.desktop

import "Items"

Control
{
    id: layoutView

    property var layout: null
    property var zoomedItem: null

    background: Rectangle { color: ColorTheme.window }

    LayoutModel
    {
        id: layoutModel
        layoutId: layout ? layout.id : NxGlobals.uuid("")
    }

    contentItem: InteractiveGrid
    {
        id: interactiveGrid

        cellAspectRatio: layout ? layout.cellAspectRatio : 1

        readonly property size cellSize:
        {
            // Calculating cell size manually to avoid a binding loop which occurs when viewport
            // margins depend on cell spacing.
            const contentSize = Qt.size(viewport.gridBounds.width, viewport.gridBounds.height)
            const size = Geometry.scaled(
                contentSize, Qt.size(width, height), Qt.KeepAspectRatio)
            return Qt.size(size.width / contentSize.width, size.height / contentSize.height)
        }

        readonly property real cellSpacing:
            layout ? Math.min(cellSize.width, cellSize.height) * layout.cellSpacing : 0

        margins: -cellSpacing / 2

        Repeater
        {
            id: repeater

            model: layoutModel

            delegate: Item
            {
                GridViewport.geometry: model.itemData.geometry

                readonly property Item contentItem: resourceItem
                ResourceItem
                {
                    id: resourceItem

                    anchors.fill: parent
                    anchors.margins: interactiveGrid.cellSpacing / 2
                    layoutItemData: model.itemData
                }
            }
        }
    }

    function getResource(resourceItem)
    {
        return resourceItem && resourceItem.layoutItemData && resourceItem.layoutItemData.resource
    }

    readonly property var resources:
        { "containsResource": resource => resource && !!layoutView.getItem(resource) }

    function getItem(resource)
    {
        if (resource)
        {
            for (let i = 0; i < repeater.count; ++i)
            {
                const item = repeater.itemAt(i).contentItem
                if (getResource(item) === resource)
                    return item
            }
        }

        return null
    }

    function containsItem(item)
    {
        if (item)
        {
            for (let i = 0; i < repeater.count; ++i)
            {
                if (repeater.itemAt(i).contentItem === item)
                    return true
            }
        }

        return false
    }

    onZoomedItemChanged:
    {
        const item = containsItem(zoomedItem) ? zoomedItem : null
        interactiveGrid.zoomedItem = item && item.parent
    }
}
