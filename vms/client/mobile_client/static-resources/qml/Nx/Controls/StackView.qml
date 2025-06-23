// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0

StackView
{
    id: stackView

    property int transitionDuration: 200

    function pushScreen(url: url, properties: variant) : QtObject
    {
        return push(url, properties, StackView.Immediate)
    }

    onBusyChanged:
    {
        if (!busy && currentItem)
            currentItem.forceActiveFocus()
    }

    NxObject
    {
        id: d

        property real scaleInXHint: -1
        property real scaleInYHint: -1
        property real maxShift: 80

        function transitionFinished(properties)
        {
            properties.exitItem.x = 0.0
            properties.exitItem.scale = 1.0
            properties.exitItem.opacity = 1.0
            scaleInXHint = -1
            scaleInYHint = -1
        }

        function getFromX()
        {
            if (scaleInXHint < 0)
                return 0

            var dx = scaleInXHint - stackView.width / 2
            var normalized = Math.max(Math.abs(dx), maxShift)
            return dx > 0 ? normalized : -normalized
        }

        function getFromY()
        {
            if (scaleInYHint < 0)
                return 0

            var dy = scaleInYHint - stackView.height / 2
            var normalized = Math.max(Math.abs(dy), maxShift)
            return dy > 0 ? normalized : -normalized
        }
    }

    replaceEnter: null
    replaceExit: null
    pushEnter: null
    pushExit: null
    popEnter: null
    popExit: null
}
