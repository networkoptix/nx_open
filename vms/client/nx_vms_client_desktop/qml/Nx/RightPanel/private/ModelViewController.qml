// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.core

import ".."

NxObject
{
    id: controller

    property bool active: true
    property var view: null
    property var model: (view && view.model) || null
    property var loggingCategory
    property alias placeholder: placeholderParameters
    property alias tileController: tileController
    property bool standardTileInteraction: true

    function updateVisibility()
    {
        d.updateData()

        if (model)
            model.setLivePaused(!view.visible)
    }

    onActiveChanged:
    {
        if (active)
            updateVisibility()
    }

    Connections
    {
        target: view
        enabled: active

        function onAtYEndChanged()
        {
            if (view.atYEnd)
                d.fetchBottom()
        }

        function onAtYBeginningChanged()
        {
            if (view.atYBeginning)
                d.fetchTop()
        }

        function onVisibleChanged()
        {
            updateVisibility()
        }
    }

    Connections
    {
        // A fix for inline header height changes causing scroll by the height delta.

        target: (view && view.headerItem) || null
        ignoreUnknownSignals: true
        enabled: active

        function onHeightChanged()
        {
            const delta = view.headerItem.height - d.savedHeaderHeight
            d.savedHeaderHeight += delta

            const scrollPos = view.contentY - view.originY

            if (delta > 0 && scrollPos - delta < 1/*px*/)
            {
                d.fixupInProgress = true
                view.positionViewAtBeginning()
                d.fixupInProgress = false
            }
        }
    }

    Connections
    {
        target: model
        enabled: active

        function onDataNeeded()
        {
            d.updateData();
        }

        function onFetchCommitStarted(request)
        {
            if (!view.visible)
                return

            function directionName(direction)
            {
                return direction === EventSearch.FetchDirection.older ? "earlier" : "later"
            }

            console.debug(loggingCategory,
                `${view.objectName}: fetch commit started, direction=${directionName(request.direction)}`)

            d.savedTopmostIndex = request.direction === EventSearch.FetchDirection.newer
                ? NxGlobals.toPersistent(model.index(0, 0, NxGlobals.invalidModelIndex()))
                : NxGlobals.invalidModelIndex()
        }

        function onFetchFinished(result, centralItemIndex, request)
        {
            if (!view.visible)
                return

            console.debug(loggingCategory, `${view.objectName}: fetch finished`)
            topPreloader.visible = false
            bottomPreloader.visible = false

            if (view instanceof TableView)
            {
                view.positionViewAtRow(centralItemIndex,
                    (request.direction === EventSearch.FetchDirection.newer)
                        ? TableView.AlignTop : TableView.AlignBottom)
            }
            else if (d.savedTopmostIndex.valid)
            {
                view.positionViewAtIndex(d.savedTopmostIndex, ListView.Beginning)
            }
            d.savedTopmostIndex = NxGlobals.invalidModelIndex()
        }

        function onAsyncFetchStarted(request)
        {
            if (request.direction === EventSearch.FetchDirection.older)
                bottomPreloader.visible = true
            else
                topPreloader.visible = true
        }
    }

    function requestUpdateIfNeeded(request, immediately)
    {
        if (d.fixupInProgress)
            return

        if (model && !model.fetchInProgress())
            model.fetchData(request, !!immediately)
    }

    TilePreloader
    {
        id: topPreloader

        width: (parent && parent.width) || 0
        parent: (view && view.headerItem) || null
    }

    TilePreloader
    {
        id: bottomPreloader

        width: (parent && parent.width) || 0
        parent: (view && view.headerItem) || null
    }

    ResultsPlaceholder
    {
        id: placeholderItem

        parent: view
        anchors.fill: parent || undefined
        anchors.topMargin: (view && view.headerItem && view.headerItem.height) || 0
        shown: model && model.placeholderRequired

        icon: placeholderParameters.icon
        title: placeholderParameters.title
        description: placeholderParameters.description
        action: placeholderParameters.action
    }

    NxObject
    {
        id: d
        property var savedTopmostIndex: NxGlobals.invalidModelIndex()
        property real savedHeaderHeight: 0
        property bool fixupInProgress: false

        function fetchTop()
        {
            if (model && model.sourceModel)
            {
                controller.requestUpdateIfNeeded(
                    model.requestForDirection(EventSearch.FetchDirection.newer))
            }
        }

        function fetchBottom()
        {
            if (model && model.sourceModel)
            {
                controller.requestUpdateIfNeeded(
                    model.requestForDirection(EventSearch.FetchDirection.older))
            }
        }

        function updateData()
        {
            if (view.atYEnd)
            {
                d.fetchBottom()
            }
            else if (view.atYBeginning)
            {
                d.fetchTop()
            }
            else
            {
                controller.requestUpdateIfNeeded(
                    EventSearchUtils.fetchRequest(EventSearch.FetchDirection.older,
                        view.currentCentralPointUs()))
            }
        }
    }

    PlaceholderParameters
    {
        id: placeholderParameters
    }

    TileController
    {
        id: tileController
    }

    Connections
    {
        target: tileController
        enabled: active

        function onCloseRequested(row)
        {
            model.removeRow(row)
        }

        function onHoverChanged(row, hovered)
        {
            model.setAutoClosePaused(row, hovered)
        }

        function onLinkActivated(row, link)
        {
            model.activateLink(row, link)
        }
    }

    Connections
    {
        target: tileController
        enabled: controller.standardTileInteraction && active

        function onClicked(row, mouseButton, keyboardModifiers)
        {
            model.click(row, mouseButton, keyboardModifiers)
        }

        function onDoubleClicked(row)
        {
            model.doubleClick(row)
        }

        function onDragStarted(row, pos, size)
        {
            model.startDrag(row, pos, size)
        }
    }

    Connections
    {
        target: tileController
        enabled: active

        function onContextMenuRequested(row, globalPos)
        {
            model.showContextMenu(row, globalPos, controller.standardTileInteraction)
        }
    }

    Component.onCompleted:
        d.savedHeaderHeight = (view && view.headerItem) ? view.headerItem.height : 0
}
