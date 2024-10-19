// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0

Rectangle
{
    id: control

    property alias loops: animation.loops
    property int loopPeriodMs: 400
    property point pos

    opacity: 0.8
    height: radius * 2
    width: radius * 2
    radius: 48
    border.width: 2
    border.color: ColorTheme.colors.light1
    color: "transparent"

    x: pos.x - width / 2
    y: pos.y - height / 2

    onVisibleChanged:
    {
        if (visible)
            baseAnimation.start()
        else
            baseAnimation.stop()
    }

    SequentialAnimation
    {
        id: baseAnimation

        property real initialRadius

        ScriptAction
        {
            script: baseAnimation.initialRadius = control.radius
        }

        NumberAnimation
        {
            id: animation

            loops: 3
            duration: control.loopPeriodMs
            target: control
            property: "radius"
            to: 0
        }

        ScriptAction
        {
            script:
            {
                control.visible = false
                control.radius = baseAnimation.initialRadius
            }
        }
    }
}
