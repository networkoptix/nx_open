// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

/**
 * A tab button that shows icon and text when selected, and only icon when not selected.
 * The transition is animated.
 */
AbstractTabButton
{
    id: tabButton

    property int animationDurationMs: 200

    property real normalWidth: Math.ceil(implicitWidth)
    property real compactWidth: leftPadding + image.width + rightPadding

    width: isCurrent ? normalWidth : compactWidth

    focusFrame.visible: false

    Behavior on width
    {
        NumberAnimation
        {
            duration: animationDurationMs
            easing.type: Easing.InOutCubic
        }
    }

    textLabel
    {
        opacity: tabButton.isCurrent ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity
        {
            NumberAnimation
            {
                duration: animationDurationMs
            }
        }
    }

    underline
    {
        x: 0
        width: tabButton.width
    }
}
