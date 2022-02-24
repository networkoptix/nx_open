// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


import QtQuick 2.6

import Nx 1.0
import nx.vms.client.desktop 1.0

Text
{
    text: performanceInfo.text
    textFormat: Text.RichText
    color: ColorTheme.highlight
    font.pixelSize: 13

    visible: performanceInfo.visible

    property var previousWindow: null

    onWindowChanged: function(window)
    {
        // Avoid counting frame renderings from previous window (if any).
        if (previousWindow)
            previousWindow.beforeRendering.disconnect(performanceInfo.registerFrame)

        window.beforeRendering.connect(performanceInfo.registerFrame)
        previousWindow = window
    }

    PerformanceInfo
    {
        id: performanceInfo
    }
}
