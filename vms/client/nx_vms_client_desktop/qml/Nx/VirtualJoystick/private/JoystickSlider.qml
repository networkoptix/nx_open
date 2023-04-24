// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx 1.0

Slider
{
    id: slider

    value: 0.5

    onPressedChanged:
    {
        if (pressed)
            timer.stop()
        else
            timer.start()
    }

    Timer
    {
        id: timer

        interval: 20
        repeat: true
        running: false

        onTriggered:
        {
            if (slider.pressed)
                return

            if (slider.value > 0.5)
            {
                if (slider.value - 0.5 <= 0.05)
                {
                    slider.value = 0.5
                    stop()
                }
                else
                {
                    slider.value -= 0.05
                }
            }

            if (slider.value < 0.5)
            {
                if (0.5 - slider.value <= 0.05)
                {
                    slider.value = 0.5
                    stop()
                }
                else
                {
                    slider.value += 0.05
                }
            }
        }
    }
}
