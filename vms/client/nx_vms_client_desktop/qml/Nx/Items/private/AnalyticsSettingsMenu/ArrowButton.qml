// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

MouseArea
{
    id: control

    // Possible states: up, down, left, right.
    property string state: "up"
    readonly property var rotations: {"up": 180, "down": 0, "left": 90, "right": -90}
    property alias icon: icon

    implicitWidth: 20
    implicitHeight: 20

    acceptedButtons: Qt.LeftButton
    hoverEnabled: true

    ArrowIcon
    {
        id: icon

        anchors.centerIn: parent
        rotation: rotations[control.state] ?? rotations.up
        color: control.containsMouse && !control.pressed
            ? ColorTheme.lighter(ColorTheme.light, 2)
            : ColorTheme.light
    }
}
