// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import nx.vms.client.desktop

import "../figure_utils.js" as F

Instrument
{
    id: instrument

    property ListModel model: null
    property real hoverDistance: 8
    property real pointRadius: 6
    property bool closed: false

    readonly property alias hoveredEdgeIndex: d.currentEdgeIndex
    readonly property alias hoveredEdgeStart: d.currentEdgeStart
    readonly property alias hoveredEdgeEnd: d.currentEdgeEnd
    readonly property bool edgeHovered: enabled && d.edgeHovered

    enabled: model && model.count >= 2

    property var d: QtObject
    {
        id: d

        property int currentEdgeIndex
        property point currentEdgeStart
        property point currentEdgeEnd
        property bool edgeHovered: false

        function normalizedRect(p1, p2)
        {
            let x1
            let y1
            let x2
            let y2

            if (p1.x < p2.x)
            {
                x1 = p1.x
                x2 = p2.x
            }
            else
            {
                x1 = p2.x
                x2 = p1.x
            }

            if (p1.y < p2.y)
            {
                y1 = p1.y
                y2 = p2.y
            }
            else
            {
                y1 = p2.y
                y2 = p1.y
            }

            return
        }

        function boundingBoxContains(p, p1, p2)
        {
            return (p.x < p1.x !== p.x < p2.x) && (p.y < p1.y !== p.y < p2.y)
        }

        function closeToPoint(p, p1)
        {
            return Math.abs(p1.x - p.x) <= pointRadius && Math.abs(p1.y - p.y) <= pointRadius
        }

        function distanceToEdge(p, p1, p2)
        {
            if (!boundingBoxContains(p, p1, p2))
                return Infinity

            const a = p2.y - p1.y
            const b = p2.x - p1.x
            const c = p2.x * p1.y - p2.y * p1.x
            return Math.abs(a * p.x - b * p.y + c) / Math.sqrt(a * a + b * b)
        }

        function getPoint(index)
        {
            const data = model.get(index)
            return Qt.point(F.absX(data.x, instrument.item), F.absY(data.y, instrument.item))
        }
    }

    onHoverMove:
    {
        if (model.count < 2)
        {
            d.edgeHovered = false
            return
        }

        let minDistance = Infinity
        let closestEdgeIndex
        let closestEdgeP1
        let closestEdgeP2

        let prev = closed ? d.getPoint(model.count - 1) : d.getPoint(0)
        for (let i = closed ? 0 : 1; i < model.count; ++i)
        {
            const current = d.getPoint(i)
            if (d.closeToPoint(hover.position, current))
            {
                d.edgeHovered = false
                return
            }

            const dist = d.distanceToEdge(hover.position, prev, current)
            if (dist < minDistance)
            {
                minDistance = dist
                closestEdgeIndex = i
                closestEdgeP1 = prev
                closestEdgeP2 = current
            }
            prev = current
        }

        d.edgeHovered = minDistance < hoverDistance
        if (d.edgeHovered)
        {
            d.currentEdgeIndex = closestEdgeIndex
            d.currentEdgeStart = closestEdgeP1
            d.currentEdgeEnd = closestEdgeP2
        }
    }

    function clear()
    {
        d.edgeHovered = false
    }
}
