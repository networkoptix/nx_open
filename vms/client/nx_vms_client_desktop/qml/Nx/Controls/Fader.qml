// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml 2.6
import QtQuick 2.6
import QtQuick.Controls 2.4

Control
{
    id: control
    property bool opaque: true
    property int fadeDuration: 200

    signal animationFinished()

    opacity: opaque ? 1.0 : 0.0
    visible: opacity > 0

    Behavior on opacity
    {
        NumberAnimation
        {
            easing.type: Easing.InOutQuad
            duration: fadeDuration

            onRunningChanged:
            {
                if (!running)
                    control.animationFinished()
            }
        }
    }
}
