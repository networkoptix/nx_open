// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Rectangle
{
    id: root

    property color from: "#272D35"
    property color to: "transparent"

    implicitWidth: parent ? parent.width : 100
    height: 80
    gradient: Gradient
    {
        GradientStop { position: 0.0; color: root.from }
        GradientStop { position: 1.0; color: root.to }
    }
    opacity: visible ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
