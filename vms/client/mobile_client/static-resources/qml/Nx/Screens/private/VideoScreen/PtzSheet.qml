// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core
import Nx.Controls

import Nx.Mobile.Controls

AdaptiveSheet
{
    id: sheet

    property alias preloaders: panel.preloaders
    property alias controller: panel.controller
    property alias customRotation: panel.customRotation
    property alias moveOnTapMode: panel.moveOnTapMode
    property alias ptz: panel
    readonly property bool available: controller.available ?? false

    title: qsTr("PTZ")
    spacing: 24
    contentSpacing: 20
    interactive: !panel.joystick.active
    extraBottomPadding: 0

    IconButton
    {
        parent: titleCustomArea

        width: 32
        height: 32

        icon.source: "image://skin/32x32/Outline/close.svg?primary=light10"
        icon.width: 32
        icon.height: 32

        onClicked: close()
    }

    PtzPanel
    {
        id: panel

        width: parent.width
    }
}
