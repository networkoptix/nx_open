// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Mobile

import nx.vms.client.core

NxObject
{
    id: controller

    property bool analyticsSearchMode: false
    property alias view: refreshWatcher.view
    property alias refreshWatcher: refreshWatcher
    property Item refreshPreloader: null

    property EventSearchModel searchModel:
        windowContext.mainSystemContext.createSearchModel(controller.analyticsSearchMode)
    property CommonObjectSearchSetup searchSetup:
        windowContext.mainSystemContext.createSearchSetup(searchModel)
    property AnalyticsSearchSetup analyticsSearchSetup: controller.analyticsSearchMode
        ? windowContext.mainSystemContext.createAnalyticsSearchSetup(searchModel)
        : null
    property BookmarkSearchSetup bookmarkSearchSetup: controller.analyticsSearchMode
        ? null
        : windowContext.mainSystemContext.createBookmarkSearchSetup(searchModel)

    function requestUpdate(direction)
    {
        if (!view
            || controller.searchModel.fetchInProgress()
            || !windowContext.sessionManager.hasConnectedSession)
        {
            return
        }

        if (direction !== EventSearch.FetchDirection.newer
            && direction !== EventSearch.FetchDirection.older)
        {
            controller.searchModel.clear()
            direction = EventSearch.FetchDirection.older
        }

        controller.searchModel.fetchData(controller.searchModel.requestForDirection(direction))
    }

    ViewUpdateWatcher
    {
        id: refreshWatcher

        onUpdateRequested: (requestType) =>
        {
            switch (requestType)
            {
                case ViewUpdateWatcher.RequestType.TopFetch:
                    controller.requestUpdate(EventSearch.FetchDirection.newer)
                    break
                case ViewUpdateWatcher.RequestType.BottomFetch:
                    controller.requestUpdate(EventSearch.FetchDirection.older)
                    break
                case ViewUpdateWatcher.RequestType.FullRefresh:
                    controller.requestUpdate()
                    break
            }
        }
    }

    Connections
    {
        target: controller.bookmarkSearchSetup

        function onParametersChanged() { controller.requestUpdate() }
    }

    Connections
    {
        target: view ? controller.searchModel : null

        function onAsyncFetchStarted(request)
        {
            d.reload = !view.count

            if (d.reload)
            {
                refreshPreloader.visible = true
                return
            }

            if (request.direction === EventSearch.FetchDirection.newer)
                view.headerItem.loading = true
            else if (request.direction === EventSearch.FetchDirection.older)
                view.footerItem.loading = true
        }

        function onFetchFinished(result, centralItemIndex, request)
        {
            refreshPreloader.visible = false

            // The fetched rows are not laid out yet, so the previously first row still has
            // index 0. It is instantiated only when it is in or near the viewport, and that is
            // exactly when the rows inserted before it push it down the screen.
            const pushedRow = view.itemAtIndex(0)
            const pushedRowY = pushedRow ? pushedRow.y - view.contentY : 0

            view.forceLayout()

            if (d.reload)
            {
                // An emptied view may still be returning to its bounds, and the rows arriving in
                // the middle of the return would stop it there.
                view.positionViewAtBeginning()
            }
            else if (pushedRow
                && request.direction === EventSearch.FetchDirection.newer
                && centralItemIndex > 0)
            {
                // Keep the pushed row where it is on the screen, also during a drag or a trackpad
                // momentum: the position set here survives the next move events. The view may
                // still be returning to the header: stopping the return here leaves the view in
                // bounds unless the fetched rows are shorter than the remaining overshoot.
                view.positionViewAtIndex(centralItemIndex, ListView.Beginning)
                view.contentY = Math.max(view.contentY - pushedRowY, view.headerItem.y)
                d.keepOnScreen(centralItemIndex)
            }

            d.reload = false

            view.headerItem.loading = false
            view.footerItem.loading = false
        }
    }

    NxObject
    {
        id: d

        property bool reload: false //< The current fetch fills an empty list.

        // The fetched delegates get their final heights from their layouts in the next polish,
        // and the rows below them move down. Puts the item back before that frame is rendered.
        // Looked up by index both times: the delegate may be gone by then if no frame was rendered
        // in between (e.g. the model was cleared while the application was in the background).
        function keepOnScreen(index)
        {
            const window = view.Window.window
            const item = view.itemAtIndex(index)
            if (!item || !window)
                return

            const y = item.y - view.contentY
            const restore = () =>
            {
                window.afterAnimating.disconnect(restore)
                const current = view.itemAtIndex(index)
                if (current)
                    view.contentY = current.y - y
            }
            window.afterAnimating.connect(restore)
        }
    }
}
