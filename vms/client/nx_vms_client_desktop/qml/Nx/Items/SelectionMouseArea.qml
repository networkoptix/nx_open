// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import nx.vms.client.desktop 1.0

MouseArea
{
    CustomCursor.cursor: CustomCursors.cross

    readonly property bool selectionInProgress: selectionStarted
    readonly property rect selectionRect: Qt.rect(
        Math.min(x1, x2), Math.min(y1, y2), Math.abs(x2 - x1), Math.abs(y2 - y1))

    signal selectionFinished()
    signal singleClicked(real x, real y)

    // Private.

    property int x1: 0
    property int y1: 0
    property int x2: 0
    property int y2: 0
    property bool selectionStarted: false
    property bool movedSinceLastPress: false

    function startSelection(x, y)
    {
        x1 = x2 = x
        y1 = y2 = y
        selectionStarted = true
    }

    function continueSelection(x, y)
    {
        x2 = x
        y2 = y
    }

    function stopSelection()
    {
        x1 = x2 = 0
        y1 = y2 = 0
        selectionStarted = false
    }

    onPressed: (mouse) =>
    {
        movedSinceLastPress = false
        if (mouse.button == Qt.LeftButton)
            startSelection(mouse.x, mouse.y)
        else if (mouse.button == Qt.RightButton)
            stopSelection()
    }

    onReleased: (mouse) =>
    {
        if (selectionStarted && mouse.button == Qt.LeftButton)
        {
            continueSelection(mouse.x, mouse.y)
            if (movedSinceLastPress)
                selectionFinished()
            else
                singleClicked(mouse.x, mouse.y)

            stopSelection()
        }
    }

    onPositionChanged: (mouse) =>
    {
        movedSinceLastPress = true
        if (selectionStarted)
            continueSelection(mouse.x, mouse.y)
    }
}
