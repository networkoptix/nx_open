// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx

/**
 * Keeps the target MouseArea in the correct state. There is a bug in Qt where MouseArea may retain
 * its pressed state even after the button is released (MouseButtonRelease event is not received by
 * MouseArea, QTBUG-123537, QTBUG-113384).
 */
Timer
{
    property MouseArea target: parent

    running: Qt.platform.os === "osx" && target.pressed
    interval: 100
    repeat: true

    onTriggered:
    {
        if (target.pressedButtons & Qt.LeftButton)
            NxGlobals.ensureMousePressed(target, Qt.LeftButton)
        if (target.pressedButtons & Qt.RightButton)
            NxGlobals.ensureMousePressed(target, Qt.RightButton)
        if (target.pressedButtons & Qt.MiddleButton)
            NxGlobals.ensureMousePressed(target, Qt.MiddleButton)
    }
}
