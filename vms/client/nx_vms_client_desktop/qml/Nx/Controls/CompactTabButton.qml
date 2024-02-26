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

    focusFrame.visible: false

    width: isCurrent
        ? Math.ceil(implicitWidth)
        : (leftPadding + image.width + spacing)
}
