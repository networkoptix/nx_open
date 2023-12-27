// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core

import nx.vms.client.desktop

import "../figure_utils.js" as F

Instrument
{
    readonly property ListModel model: pointsModel
    readonly property alias count: pointsModel.count

    readonly property real startX: count > 0 ? getPoint(0).x : 0
    readonly property real startY: count > 0 ? getPoint(0).y : 0
    readonly property real endX: count > 0 ? getPoint(count - 1).x : 0
    readonly property real endY: count > 0 ? getPoint(count - 1).y : 0

    property real snapDistance: 12
    property int minPoints: closed ? 3 : 2
    property int maxPoints: 0
    property bool closed: false

    signal pointFinished()

    readonly property var hoveredPoint:
        enabled && d.hoveredPointIndex >= 0 ? getPoint(d.hoveredPointIndex) : undefined

    enabled: false
    hoverEnabled: true

    property var d: NxObject
    {
        id: d

        property int hoveredPointIndex: -1

        ListModel
        {
            id: pointsModel

            function makePoint(p)
            {
                return Qt.point(F.relX(p.x, item), F.relY(p.y, item))
            }

            function shouldFinish()
            {
                if (count === 0)
                    return false

                const p1 = get(count - 1)
                if (closed)
                {
                    const p2 = get(0)
                    if (sameCoordinates(p1.x, p2.x) && sameCoordinates(p1.y, p2.y))
                        return true
                }

                if (count > 1)
                {
                    const p3 = get(count - 2)
                    if (sameCoordinates(p1.x, p3.x) && sameCoordinates(p1.y, p3.y))
                        return true
                }

                return false
            }
        }

        function snappedToEnd(p)
        {
            if (count === 0)
                return p

            const fp = getPoint(0)
            const lp = count > 1 ? getPoint(count - 2) : fp

            if (!closed || F.distance(p.x, p.y, lp.x, lp.y) < F.distance(p.x, p.y, fp.x, fp.y))
                return F.snapped(p.x, p.y, lp.x, lp.y, snapDistance)
            else
                return F.snapped(p.x, p.y, fp.x, fp.y, snapDistance)
        }

        function isLastPointSnappedTo(index)
        {
            if (count < 2 || index >= count - 1)
                return false

            const p1 = getPoint(count - 1)
            const p2 = getPoint(index)
            return sameCoordinates(p1.x, p2.x) && sameCoordinates(p1.y, p2.y)
        }

        function lastTwoPointsCollapsed()
        {
            return isLastPointSnappedTo(count - 2)
        }

        function processMove(event)
        {
            event.accepted = true
            var hoveredIndex = -1
            if (count > 0)
            {
                setPoint(count - 1,
                    count > minPoints ? snappedToEnd(event.position) : event.position)

                if (closed && isLastPointSnappedTo(0))
                    hoveredIndex = 0
                else if (isLastPointSnappedTo(count - 2))
                    hoveredIndex = count - 2
            }

            d.hoveredPointIndex = hoveredIndex
        }

        function normalizedIndex(index)
        {
            if (count === 0)
                return index

            while (index < 0)
                index += count

            while (index >= count)
                index -= count

            return index
        }
    }

    onMousePress: (mouse) =>
    {
        mouse.accepted = true

        if (count === 0)
        {
            pointsModel.append(pointsModel.makePoint(mouse.position))
            pointsModel.append(pointsModel.makePoint(mouse.position))
            return
        }
        d.processMove(mouse)
    }

    onMouseMove: (mouse) => d.processMove(mouse)
    onHoverMove: (hover) => d.processMove(hover)

    onMouseRelease: (mouse) =>
    {
        mouse.accepted = true

        d.processMove(mouse)

        if (count === 2)
        {
            const p1 = getPoint(0)
            const p2 = getPoint(1)

            if (F.distance(p1.x, p1.y, p2.x, p2.y) < snapDistance)
                return
        }

        if (count > minPoints && pointsModel.shouldFinish())
        {
            removePoint(count - 1)
            finish()
            return
        }

        if (d.lastTwoPointsCollapsed())
            return

        if (count === maxPoints)
        {
            finish()
            return
        }

        pointsModel.append(pointsModel.makePoint(mouse))
    }

    function start()
    {
        clear()
        enabled = true
    }

    function finish()
    {
        enabled = false
    }

    function getPoint(index)
    {
        const p = pointsModel.get(d.normalizedIndex(index))
        return Qt.point(F.absX(p.x, item), F.absY(p.y, item))
    }

    function getPoints()
    {
        var result = []

        for (var i = 0; i < pointsModel.count; ++i)
            result.push(getPoint(i))

        return result
    }

    function setPoint(index, p)
    {
        setRelativePoint(index, pointsModel.makePoint(p))
    }

    function setPointX(index, x)
    {
        setRelativePointX(index, F.relX(x, item))
    }

    function setPointY(index, y)
    {
        setRelativePointY(index, F.relY(y, item))
    }

    function setPoints(points)
    {
        if (pointsModel.count !== points.length)
        {
            pointsModel.clear()
            for (var i = 0; i < points.length; ++i)
                pointsModel.append(pointsModel.makePoint(points[i]))
        }
        else
        {
            for (i = 0; i < points.length; ++i)
                setPoint(i, points[i])
        }
    }

    function insertPoint(index, p)
    {
        pointsModel.insert(index, pointsModel.makePoint(p))
    }

    function removePoint(index)
    {
        pointsModel.remove(index)

        if (d.hoveredPointIndex === index)
            d.hoveredPointIndex = -1
    }

    function setRelativePoint(index, p)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (differentCoordinates(old.x, p.x))
            pointsModel.setProperty(index, "x", p.x)
        if (differentCoordinates(old.y, p.y))
            pointsModel.setProperty(index, "y", p.y)
    }

    function setRelativePointX(index, x)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (differentCoordinates(old.x, x))
            pointsModel.setProperty(index, "x", x)
    }

    function setRelativePointY(index, y)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (differentCoordinates(old.y, y))
            pointsModel.setProperty(index, "y", y)
    }

    function setRelativePoints(points)
    {
        if (pointsModel.count !== points.length)
        {
            pointsModel.clear()
            for (var i = 0; i < points.length; ++i)
                pointsModel.append(points[i])
        }
        else
        {
            for (i = 0; i < points.length; ++i)
                setRelativePoint(i, points[i])
        }
    }

    function getRelativePoints()
    {
        var result = []

        for (var i = 0; i < pointsModel.count; ++i)
        {
            const p = pointsModel.get(i)
            result.push(Qt.point(p.x, p.y))
        }

        return result
    }

    function clear()
    {
        pointsModel.clear()
        d.hoveredPointIndex = -1
    }

    function sameCoordinates(left, right)
    {
        return Math.abs(left - right) < 0.000000001
    }

    function differentCoordinates(left, right)
    {
        return !sameCoordinates(left, right)
    }
}
