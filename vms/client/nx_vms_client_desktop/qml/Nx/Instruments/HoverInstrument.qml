// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.desktop

Instrument
{
    id: instrument

    readonly property alias hovered: d.hovered
    readonly property alias position: d.position

    hoverEnabled: true

    onHoverEnter: (event) =>
    {
        d.position = event.localPosition
        d.hovered = true
    }

    onHoverLeave:
        d.hovered = false

    onHoverMove: (event) =>
        d.position = event.localPosition

    readonly property QtObject d: QtObject
    {
        id: d

        property bool hovered: false
        property point position
    }
}
