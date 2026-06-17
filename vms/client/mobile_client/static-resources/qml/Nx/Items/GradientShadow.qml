// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Item
{
    id: root

    property color from: "#272D35"
    property color to: "transparent"

    // Distance left to the corresponding scroll edge. Together with `fadeZone`
    // controls how opaque the shadow is.
    property real distance: 0

    // Distance over which the shadow fades from fully transparent to fully opaque.
    // When 0 the fade is disabled and the shadow stays fully opaque (static usage).
    property real fadeZone: 0

    opacity: fadeZone > 0 ? MathUtils.bound(0, distance / fadeZone, 1) : 1
    visible: opacity > 0

    implicitWidth: parent ? parent.width : 100
    implicitHeight: 80
    clip: true

    Rectangle
    {
        color: root.from
        width: parent.width
        height: root.height - shadow.height
        visible: height > 0
    }

    Rectangle
    {
        id: shadow

        height: Math.min(80, parent.height)
        width: parent.width
        anchors.bottom: parent.bottom

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: root.from }
            GradientStop { position: 1.0; color: root.to }
        }
    }
}
