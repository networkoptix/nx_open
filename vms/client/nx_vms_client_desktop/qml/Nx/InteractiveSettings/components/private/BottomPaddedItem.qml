// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

/*
 * This item has isGroup property and applies additional bottom padding
 * to itself if either its or next item in a positioner isGroup is true
 */
FocusScope
{
    property bool isGroup: false
    property alias paddedItem: control.contentItem
    property real conditionalBottomPadding: 8

    implicitWidth: control.implicitWidth
    implicitHeight: control.implicitHeight

    Control
    {
        id: control

        padding: 0
        bottomPadding: needsBottomPadding ? conditionalBottomPadding : 0
        anchors.fill: parent
    }

    readonly property bool needsBottomPadding:
    {
        if (!Positioner || !parent || Positioner.isLastItem)
            return false

        var next = parent.children[Positioner.index + 1]
        return next && (isGroup || next.isGroup) || false
    }
}
