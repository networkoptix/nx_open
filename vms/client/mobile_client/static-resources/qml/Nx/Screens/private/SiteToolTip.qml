// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

ToolTip
{
    id: control

    required property ToolBar toolBar
    readonly property bool available: !!text

    parent: toolBar
    width: parent.width - 40
    x: (parent.width - width) / 2
    y: parent.height - 10

    Connections
    {
        target: control.toolBar
        enabled: control.available

        function onToolBarClicked() { control.open() }
    }
}
