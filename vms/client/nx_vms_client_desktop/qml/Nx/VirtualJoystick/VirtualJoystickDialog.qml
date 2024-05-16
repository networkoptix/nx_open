// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls

import "private"

RowLayout
{
    signal horizontalStickMoved(real value)
    signal verticalStickMoved(real value)
    signal buttonPressed(int buttonIndex)
    signal buttonReleased(int buttonIndex)

    Layout.alignment: Qt.AlignVCenter

    JoystickSlider
    {
        orientation: Qt.Horizontal

        onValueChanged: horizontalStickMoved(value - 0.5)
    }

    JoystickSlider
    {
        orientation: Qt.Vertical

        onValueChanged: verticalStickMoved(value - 0.5)
    }

    GridLayout
    {
        columns: 2

        Repeater {
            model: 10
            Button {
                text: index.toString()
                onPressed: buttonPressed(index)
                onReleased: buttonReleased(index)
            }
        }
    }
}
