// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Core.Ui 1.0

NxObject
{
    id: watcher

    property alias view: connections.target
    property real threshold: 0.3
    property real refreshProgress: 0.0
    readonly property real topOvershoot: 46
    readonly property real bottomOvershoot: 46
    readonly property real overshoot: Math.abs(view.verticalOvershoot)

    signal updateRequested(int requestType)

    enum RequestType
    {
        None,
        TopFetch,
        BottomFetch,
        FullRefresh
    }

    Connections
    {
        id: connections

        enabled: target && target.enabled && target.visible

        function onVerticalOvershootChanged()
        {
            d.handleOvershootChanged(target.verticalOvershoot)
        }
    }

    QtObject
    {
        id: d

        readonly property bool pressed: watcher.view.draggingVertically
        readonly property bool watching: connections.enabled
        property int requestType: ViewUpdateWatcher.RequestType.None
        property bool requested: false //< Until the view is back within its bounds.

        // The flag is cleared by the overshoot changes, which are not seen while not watching.
        onWatchingChanged: d.requested = false

        onPressedChanged:
        {
            if (d.pressed)
            {
                d.requested = false
                return
            }

            if (watcher.refreshProgress >= 1.0)
                d.requestType = ViewUpdateWatcher.RequestType.FullRefresh

            // Request on release rather than on the way back: whatever the update shows at the
            // edge (e.g. a preloader) extends the bounds before the view starts returning to them.
            d.handleOvershootChanged(watcher.view.verticalOvershoot)
        }

        function handleOvershootChanged(overshoot)
        {
            // Requested already: nothing to do until the view is back within its bounds. Within
            // the thresholds is not enough: what the request shows at the edge moves the overshoot
            // inward, and an outbound flick passes through there.
            if (d.requested)
            {
                if (Math.abs(overshoot) < 0.5)
                    d.requested = false
                return
            }

            if (overshoot < -watcher.topOvershoot)
            {
                const maxTopOvershoot = watcher.view.height * watcher.threshold
                    - watcher.topOvershoot
                const progress = overshoot > -watcher.topOvershoot
                    ? 0
                    : Math.abs(overshoot + watcher.topOvershoot) / maxTopOvershoot
                watcher.refreshProgress = Math.min(progress, 1.0)
                if (d.requestType !== ViewUpdateWatcher.RequestType.FullRefresh
                    && overshoot < -watcher.topOvershoot)
                {
                    d.requestType = ViewUpdateWatcher.RequestType.TopFetch
                }
            }
            else if (overshoot > watcher.bottomOvershoot)
            {
                d.requestType = ViewUpdateWatcher.RequestType.BottomFetch
            }
            else if (d.requestType !== ViewUpdateWatcher.RequestType.None)
            {
                watcher.refreshProgress = 0
                d.requestType = ViewUpdateWatcher.RequestType.None
            }

            // An overshoot without a drag (flick, wheel, momentum) requests on the way out: the
            // room for whatever the update shows at the edge is there already.
            if (d.pressed || d.requestType === ViewUpdateWatcher.RequestType.None)
                return

            // Reset before the request: its handler may move the view and get back here.
            const requestType = d.requestType
            watcher.refreshProgress = 0
            d.requestType = ViewUpdateWatcher.RequestType.None
            d.requested = true
            watcher.updateRequested(requestType)
        }
    }
}
