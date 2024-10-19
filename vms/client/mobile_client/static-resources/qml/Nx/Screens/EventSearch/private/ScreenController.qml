// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Mobile
import Nx.Models

import QtQuick.Controls //< remove me

import nx.vms.client.core 1.0

NxObject
{
    id: controller

    property bool analyticsSearchMode: false
    property alias view: refreshWatcher.view
    property alias refreshWatcher: refreshWatcher
    property Item refreshPreloader: null

    property EventSearchModel searchModel: createSearchModel(controller.analyticsSearchMode)
    property CommonObjectSearchSetup searchSetup: createSearchSetup(searchModel)
    property AnalyticsSearchSetup analyticsSearchSetup: controller.analyticsSearchMode
        ? createAnalyticsSearchSetup(searchModel)
        : null

    function requestUpdate(direction)
    {
        if (!view || controller.searchModel.fetchInProgress())
            return

        if (direction === EventSearch.FetchDirection.newer
            || direction === EventSearch.FetchDirection.older)
        {
            if (direction === EventSearch.FetchDirection.newer)
            {
                view.positionViewAtBeginning()
                view.currentIndex = 0 //< Select the first item position correcly.
            }
            else
            {
                view.positionViewAtEnd()
                view.currentIndex = view.count - 1 //< Select the last item position correcly.
            }
        }
        else
        {
            view.currentIndex = -1
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
        target: view ? controller.searchModel : null

        function onAsyncFetchStarted(request)
        {
            if (!view.count)
                refreshPreloader.visible = true
            else if (request.direction === EventSearch.FetchDirection.newer)
                view.headerItem.visible = true
            else if (request.direction === EventSearch.FetchDirection.older)
                view.footerItem.visible = true
        }

        function onFetchFinished(result, centralItemIndex, request)
        {
            refreshPreloader.visible = false
            view.headerItem.visible = false
            view.footerItem.visible = false

            const initialPosition = request.direction === EventSearch.FetchDirection.newer
                ? ListView.Beginning
                : ListView.End

            view.positionViewAtIndex(centralItemIndex, initialPosition)

            const targetPosition = request.direction === EventSearch.FetchDirection.newer
                ? ListView.End
                : ListView.Beginning

            d.goToPosAnimation.from = view.contentY
            view.positionViewAtIndex(centralItemIndex, targetPosition)
            d.goToPosAnimation.to = view.contentY
            d.goToPosAnimation.running = true
        }
    }

    NxObject
    {
        id: d

        readonly property real kPreloaderHeight: refreshWatcher.topOvershoot
        property NumberAnimation goToPosAnimation:
            NumberAnimation
            {
                id: goToPositionAnimation
                target: view
                property: "contentY"
                duration: 500
            }
        property ModelDataAccessor accessor:
            ModelDataAccessor
            {
                model: controller.searchModel
            }

    }

    Component.onCompleted:
    {
        controller.requestUpdate()
    }
}
