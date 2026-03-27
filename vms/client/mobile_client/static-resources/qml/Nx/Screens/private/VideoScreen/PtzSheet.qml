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
    alwaysShowCloseButton: true
    spacing: 24
    contentSpacing: 20
    interactive: !panel.joystick.active
    extraBottomPadding: 0

    PtzPanel
    {
        id: panel

        width: parent.width
    }
}
