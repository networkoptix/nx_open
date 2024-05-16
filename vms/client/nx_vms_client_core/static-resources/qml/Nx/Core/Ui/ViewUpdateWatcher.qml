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
    readonly property real topOvershoot: 36
    readonly property real bottomOvershoot: 36
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
        property int requestType: ViewUpdateWatcher.RequestType.None

        onPressedChanged:
        {
            if (!d.pressed && watcher.refreshProgress >= 1.0)
                d.requestType = ViewUpdateWatcher.RequestType.FullRefresh
        }

        function handleOvershootChanged(overshoot)
        {
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
                if (d.requestType !== ViewUpdateWatcher.RequestType.None && !d.pressed)
                    watcher.updateRequested(d.requestType)

                watcher.refreshProgress = 0
                d.requestType = ViewUpdateWatcher.RequestType.None
            }
        }
    }
}
