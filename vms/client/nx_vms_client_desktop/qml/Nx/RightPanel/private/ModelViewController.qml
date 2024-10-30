// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core

import nx.vms.client.desktop
import nx.vms.client.core

import ".."

NxObject
{
    id: controller

    property bool enabled: true
    property var view: null
    property var model: view?.model || null
    property var loggingCategory
    property alias placeholder: placeholderParameters
    property alias tileController: tileController
    property bool standardTileInteraction: true

    onEnabledChanged:
    {
        if (!controller.view)
        {
            console.assert(!controller.enabled)
            return
        }

        console.debug(loggingCategory, `${controller.view.objectName}: controller is `
            + `${controller.enabled ? "enabled" : "disabled"}`)

        if (controller.enabled)
            d.updateLive()
    }

    Connections
    {
        target: view
        enabled: controller.enabled

        function onAtYEndChanged()
        {
            if (view.atYEnd)
            {
                console.debug(loggingCategory, `${view.objectName}: scrolled to the end`)
                d.updateData()
            }
        }

        function onAtYBeginningChanged()
        {
            if (view.atYBeginning)
            {
                console.debug(loggingCategory, `${view.objectName}: scrolled to the beginning`)
                d.updateData()
            }
        }

        function onVisibleChanged()
        {
            d.updateData()
            d.updateLive()
        }
    }

    Connections
    {
        // A fix for inline header height changes causing scroll by the height delta.

        target: view?.headerItem || null
        ignoreUnknownSignals: true
        enabled: controller.enabled

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
        enabled: controller.enabled

        function onDataNeeded()
        {
            console.debug(loggingCategory, `${view.objectName}: data needed`)
            d.updateData()
        }

        function onFetchCommitStarted(request)
        {
            console.debug(loggingCategory, `${view.objectName}: fetch commit started, `
                + `direction=${d.directionName(request.direction)}`)
        }

        function onFetchFinished(result, centralItemIndex, request)
        {
            console.debug(loggingCategory, `${view.objectName}: fetch finished, `
                + `direction=${d.directionName(request.direction)}, POI=${centralItemIndex}`)

            topPreloader.visible = false
            bottomPreloader.visible = false

            if (view instanceof TableView)
            {
                view.positionViewAtRow(centralItemIndex,
                    request.direction === EventSearch.FetchDirection.newer
                        ? TableView.AlignTop
                        : TableView.AlignBottom,
                    -view.topMargin)
            }
            else if (view instanceof GridView)
            {
                view.positionViewAtIndex(centralItemIndex,
                    request.direction === EventSearch.FetchDirection.newer
                        ? GridView.Beginning
                        : GridView.End)
            }
            else if (view instanceof ListView)
            {
                view.positionViewAtIndex(centralItemIndex,
                    request.direction === EventSearch.FetchDirection.newer
                        ? ListView.Beginning
                        : ListView.End)
            }
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

        width: parent?.width || 0
        parent: view?.headerItem || null
    }

    TilePreloader
    {
        id: bottomPreloader

        width: parent?.width || 0
        parent: view?.headerItem || null
    }

    ResultsPlaceholder
    {
        id: placeholderItem

        parent: view
        anchors.fill: parent || undefined
        anchors.topMargin: view?.headerItem?.height || 0
        shown: model && model.placeholderRequired

        icon: placeholderParameters.icon
        title: placeholderParameters.title
        description: placeholderParameters.description
        action: placeholderParameters.action
    }

    NxObject
    {
        id: d

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
            if (!controller.enabled || !view.visible)
                return

            function whereAt()
            {
                if (view.atYEnd && view.atYBeginning)
                    return "whole view"
                if (view.atYEnd)
                    return "at the end"
                if (view.atYBeginning)
                    return "at the beginning"
                return "in the middle"
            }

            console.debug(loggingCategory, `${view.objectName}: update request, ${whereAt()}`)

            if (view.atYEnd)
            {
                d.fetchBottom()
            }
            else if (view.atYBeginning)
            {
                d.fetchTop()
            }
        }

        function updateLive()
        {
            if (model && controller.enabled)
                model.setLivePaused(!view.visible)
        }

        function directionName(direction)
        {
            return direction === EventSearch.FetchDirection.older ? "older" : "newer"
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
        enabled: controller.enabled

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

        function onContextMenuRequested(row, globalPos)
        {
            model.showContextMenu(row, globalPos, controller.standardTileInteraction)
        }
    }

    Connections
    {
        target: tileController
        enabled: controller.standardTileInteraction && controller.enabled

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

    Component.onCompleted:
        d.savedHeaderHeight = (view && view.headerItem) ? view.headerItem.height : 0
}
