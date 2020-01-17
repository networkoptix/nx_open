import QtQuick 2.0
import nx.client.desktop 1.0
import Nx 1.0

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
    property int minPoints: 2
    property int maxPoints: 0
    property bool closed: false

    signal pointFinished()

    enabled: false

    property var d: Object
    {
        id: d

        ListModel
        {
            id: pointsModel

            function makePoint(p)
            {
                return Qt.point(F.relX(p.x, item), F.relY(p.y, item))
            }

            function shouldFinish()
            {
                if (!count)
                    return false

                const p1 = get(count - 1)
                if (closed)
                {
                    const p2 = get(0)
                    if (p1.x === p2.x && p1.y === p2.y)
                        return true
                }

                if (count > 1)
                {
                    const p3 = get(count - 2)
                    if (p1.x === p3.x && p1.y === p3.y)
                        return true
                }

                return false
            }
        }

        function snappedToFirstPoint(point)
        {
            if (count <= minPoints)
                return point

            const fp = getPoint(0)
            return F.snapped(point.x, point.y, fp.x, fp.y, snapDistance)
        }

        function processMove(event)
        {
            event.accepted = true
            if (count > 0)
                setPoint(count - 1, snappedToFirstPoint(event.position))
        }

        function normalizedIndex(index)
        {
            while (index < 0)
                index += count

            while (index >= count)
                index -= count

            return index
        }
    }

    onMousePress:
    {
        mouse.accepted = true

        if (count === 0)
        {
            pointsModel.append(pointsModel.makePoint(d.snappedToFirstPoint(mouse.position)))
            return
        }
        d.processMove(mouse)
    }
    onMouseMove: d.processMove(mouse)
    onHoverMove: d.processMove(hover)
    onMouseRelease:
    {
        mouse.accepted = true

        d.processMove(mouse)

        if (count > minPoints && pointsModel.shouldFinish())
        {
            pointsModel.remove(count - 1)
            finish()
            return
        }

        if (count == maxPoints)
        {
            finish()
            return
        }

        pointsModel.append(pointsModel.makePoint(d.snappedToFirstPoint(mouse.position)))
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
    }

    function setRelativePoint(index, p)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (old.x !== p.x)
            pointsModel.setProperty(index, "x", p.x)
        if (old.y !== p.y)
            pointsModel.setProperty(index, "y", p.y)
    }

    function setRelativePointX(index, x)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (old.x !== x)
            pointsModel.setProperty(index, "x", x)
    }

    function setRelativePointY(index, y)
    {
        index = d.normalizedIndex(index)

        const old = pointsModel.get(index)
        if (old.y !== y)
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
    }
}
