// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core

import nx.client.core
import nx.vms.client.desktop

Instrument
{
    id: instrument

    property int dragButtons: Qt.LeftButton
    property int cancellingButtons: Qt.RightButton
    property real dragThreshold: Application.styleHints.startDragDistance

    readonly property alias dragging: d.dragging
    readonly property alias pressPosition: d.pressPosition
    readonly property alias position: d.position
    readonly property alias activeTranslation: d.activeTranslation

    signal started()
    signal finished()
    signal canceled()

    function cancel()
    {
        if (!d.dragging)
            return

        d.canceled = true
        d.dragging = false
        canceled()
    }

    onMousePress: (mouse) =>
    {
        if (d.dragging && (cancellingButtons & mouse.button))
        {
            cancel()
            return
        }

        if (!(dragButtons & mouse.button))
            return

        d.canceled = false
        d.pressPosition = mouse.position
        d.position = mouse.position
    }

    onMouseRelease: (mouse) =>
    {
        mouse.accepted = d.dragging || d.canceled

        if (dragButtons & mouse.button)
            d.canceled = false

        if (!d.dragging)
            return

        d.position = mouse.position
        d.dragging = false
        finished()
    }

    onMouseMove: (mouse) =>
    {
        if (!d.canceled)
            d.position = mouse.position

        mouse.accepted = d.dragging || d.canceled
    }

    onEnabledChanged: cancel()
    onItemChanged: cancel()

    readonly property var _d: QtObject
    {
        id: d

        property bool dragging: false
        property bool canceled: false

        property point pressPosition
        property point position
        property point activeTranslation

        onPositionChanged:
        {
            if (canceled)
                return

            activeTranslation = Geometry.cwiseSub(position, pressPosition)
            if (dragging || Geometry.length(activeTranslation) < instrument.dragThreshold)
                return

            dragging = true
            instrument.started()
        }
    }
}
