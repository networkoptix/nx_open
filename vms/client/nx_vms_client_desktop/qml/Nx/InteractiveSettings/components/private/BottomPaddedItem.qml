// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

/*
 * This item has isGroup property and applies additional bottom padding
 * to itself if either its or next item in a positioner isGroup is true
 */
Control
{
    id: control

    property real conditionalBottomPadding: 8
    property var nextItem: null

    padding: 0
    bottomPadding: needsBottomPadding ? conditionalBottomPadding : 0

    readonly property bool needsBottomPadding:
        !!nextItem && !!contentItem && (contentItem.isGroup || nextItem.isGroup) || false
}
