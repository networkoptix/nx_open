// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0

Rectangle
{
    id: control

    implicitWidth: parent ? parent.width : 100
    height: 80
    gradient: Gradient
    {
        GradientStop { position: 0.0; color: "#272D35" }
        GradientStop { position: 1.0; color: "transparent" }
    }
    opacity: visible ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
