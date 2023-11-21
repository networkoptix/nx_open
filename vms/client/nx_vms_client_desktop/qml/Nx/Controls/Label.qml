// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0

import nx.vms.client.core
import nx.vms.client.desktop 1.0

Text
{
    color: ColorTheme.windowText

    font.pixelSize: FontConfig.normal.pixelSize

    MouseArea
    {
        anchors.fill: parent
        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        CursorOverride.shape: cursorShape
        CursorOverride.active: containsMouse
    }
}
