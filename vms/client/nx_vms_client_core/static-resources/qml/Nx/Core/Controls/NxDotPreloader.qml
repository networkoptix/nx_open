// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

BusyIndicator
{
    id: control

    property int dotsCount: 3
    property int dotAnimationOffsetMs: 200
    property color color: ColorTheme.colors.dark9
    property color borderColor: color
    property real borderWidth: 0
    property real dotRadius: 4

    visible: running
    spacing: 6

    contentItem: Row
    {
        spacing: control.spacing

        Repeater
        {
            model: control.dotsCount

            delegate: Rectangle
            {
                id: dot

                radius: control.dotRadius
                width: radius * 2
                height: width
                color: control.color
                border.color: control.borderColor
                border.width: control.borderWidth
                opacity: 0

                SequentialAnimation on opacity
                {
                    id: opacityAnimation
                    running: control.running

                    PauseAnimation
                    {
                        duration: index * control.dotAnimationOffsetMs
                    }

                    SequentialAnimation
                    {
                        loops: Animation.Infinite

                        PropertyAnimation
                        {
                            duration: 500
                            easing.type: Easing.Linear
                            to: 1
                        }

                        PropertyAnimation
                        {
                            duration: 500
                            easing.type: Easing.Linear
                            to: 0
                        }

                        PauseAnimation
                        {
                            duration: control.dotAnimationOffsetMs * (control.dotsCount - 1)
                        }
                    }
                }
            }
        }
    }
}
