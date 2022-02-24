// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx 1.0

import nx.vms.client.desktop 1.0

import ".."

NxObject
{
    id: controller

    property var view: null
    property var loggingCategory
    property alias placeholder: placeholderParameters
    property alias tileController: tileController
    property bool standardTileInteraction: true

    Connections
    {
        target: view

        function onAtYEndChanged()
        {
            if (view.atYEnd)
                controller.requestFetchIfNeeded()
        }

        function onAtYBeginningChanged()
        {
            if (view.atYBeginning)
                controller.requestFetchIfNeeded()
        }

        function onVisibleChanged()
        {
            controller.requestFetchIfNeeded()

            if (view.model)
                view.model.setLivePaused(!visible)
        }
    }

    Connections
    {
        // A fix for inline header height changes causing scroll by the height delta.

        target: view ? view.headerItem : null
        ignoreUnknownSignals: true

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
        target: (view && view.model) || null

        function onDataNeeded()
        {
            controller.requestFetchIfNeeded(/*immediately*/ true)
        }

        function onLiveAboutToBeCommitted()
        {
            console.debug(loggingCategory, `${view.objectName}: live data is about to be committed`)
            controller.updateFetchDirection()
        }

        function onFetchCommitStarted(direction)
        {
            function directionName(direction)
            {
                return direction === RightPanel.FetchDirection.earlier ? "earlier" : "later"
            }

            console.debug(loggingCategory,
                `${view.objectName}: fetch commit started, direction=${directionName(direction)}`)

            d.savedTopmostIndex = direction === RightPanel.FetchDirection.later
                ? NxGlobals.toPersistent(view.model.index(0, 0, NxGlobals.invalidModelIndex()))
                : NxGlobals.invalidModelIndex()
        }

        function onFetchFinished()
        {
            console.debug(loggingCategory, `${view.objectName}: fetch finished`)
            topPreloader.visible = false
            bottomPreloader.visible = false

            if (!d.savedTopmostIndex.valid)
                return

            view.positionViewAtIndex(d.savedTopmostIndex.row, ListView.Beginning)
            d.savedTopmostIndex = NxGlobals.invalidModelIndex()
        }

        function onAsyncFetchStarted(direction)
        {
            if (direction === RightPanel.FetchDirection.earlier)
                bottomPreloader.visible = true
            else
                topPreloader.visible = true
        }
    }

    function updateFetchDirection()
    {
        if (!view || !view.model)
            return false

        if (view.atYEnd || count === 0)
        {
            view.model.setFetchDirection(RightPanel.FetchDirection.earlier)
            return true
        }

        if (view.atYBeginning)
        {
            view.model.setFetchDirection(RightPanel.FetchDirection.later)
            return true
        }

        // Neither at the beginning nor at the end.
        view.model.setFetchDirection(RightPanel.FetchDirection.earlier)
        return false
    }

    function requestFetchIfNeeded(immediately)
    {
        if (d.fixupInProgress)
            return

        if (view && view.visible && !view.model.fetchInProgress() && updateFetchDirection())
            view.model.requestFetch(!!immediately)
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
        shown: view && view.model && view.model.placeholderRequired

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

        function onCloseRequested(row)
        {
            view.model.removeRow(row)
        }

        function onHoverChanged(row, hovered)
        {
            view.model.setAutoClosePaused(row, hovered)
        }

        function onLinkActivated(row, link)
        {
            view.model.activateLink(row, link)
        }
    }

    Connections
    {
        target: tileController
        enabled: controller.standardTileInteraction

        function onClicked(row, mouseButton, keyboardModifiers)
        {
            view.model.click(row, mouseButton, keyboardModifiers)
        }

        function onDoubleClicked(row)
        {
            view.model.doubleClick(row)
        }

        function onDragStarted(row, pos, size)
        {
            view.model.startDrag(row, pos, size)
        }
    }

    Connections
    {
        target: tileController

        function onContextMenuRequested(row, globalPos)
        {
            view.model.showContextMenu(row, globalPos, controller.standardTileInteraction)
        }
    }

    Component.onCompleted:
        d.savedHeaderHeight = (view && view.headerItem) ? view.headerItem.height : 0
}
