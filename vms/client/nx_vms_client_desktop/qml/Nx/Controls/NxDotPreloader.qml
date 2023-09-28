// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

BusyIndicator
{
    id: control

    property int dotsCount: 3
    property int dotAnimationOffsetMs: 200
    property color color: "white"
    property color borderColor: "black"
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

            delegate: NxCircle
            {
                id: dot

                radius: control.dotRadius
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
