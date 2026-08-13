// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls

ToolTip
{
    id: control

    required property ToolBar toolBar
    readonly property bool available: !!text

    target: toolBar
    width: toolBar.width - 40
    targetSpacing: -10

    padding: 12

    timeout: 2000
    closePolicy: ToolTip.CloseOnEscape
        | ToolTip.CloseOnPressOutsideParent
        | ToolTip.CloseOnReleaseOutsideParent

    contentItem: Text
    {
        text: control.text
        color: ColorTheme.colors.dark4
        font.pixelSize: 14
        lineHeightMode: Text.FixedHeight
        lineHeight: 21
        elide: Text.ElideRight
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
    }

    onAvailableChanged: close()

    Connections
    {
        target: control.toolBar
        enabled: control.available

        function onToolBarClicked() { control.open() }
    }
}
