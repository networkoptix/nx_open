import QtQuick 2.10
import QtQml 2.3
import nx.vms.client.core 1.0
import Nx 1.0

import "../figure_utils.js" as F

Figure
{
    id: figure

    property alias maxPoints: pointMakerInstrument.maxPoints
    property real snapDistance: 8
    readonly property bool hasFigure: pointMakerInstrument.count > 2
        || (pointMakerInstrument.enabled && pointMakerInstrument.count > 0)

    clip: true

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: enabled
        cursorShape: (dragInstrument.enabled || dragInstrument.dragging)
            ? Qt.SizeAllCursor : Qt.ArrowCursor
    }

    PointMakerInstrument
    {
        id: pointMakerInstrument

        item: mouseArea
        closed: true

        enabled: false
    }

    PathHoverInstrument
    {
        id: hoverInstrument
        model: pointMakerInstrument.model
        enabled: !pointMakerInstrument.enabled
        item: mouseArea
    }

    FigureDragInstrument
    {
        id: dragInstrument
        pointMakerInstrument: pointMakerInstrument
        pathHoverInstrument: hoverInstrument
        forcefullyHovered: pathUtil.contains(
            Qt.point(F.relX(mouseArea.mouseX, mouseArea), F.relY(mouseArea.mouseY, mouseArea)))
        item: mouseArea
        target: figure
    }

    PathUtil
    {
        id: pathUtil
    }

    Connections
    {
        target: pointMakerInstrument.model
        onDataChanged: refresh()
        onRowsInserted: refresh()
        onRowsRemoved: refresh()
        onModelReset: refresh()
    }

    property PointGrip lastHoveredGrip: null

    Canvas
    {
        id: canvas

        anchors.fill: parent

        onPaint:
        {
            const points = pointMakerInstrument.getPoints()

            var ctx = getContext("2d")
            ctx.reset()

            if (points.length < 3)
                return

            ctx.strokeStyle = color
            ctx.fillStyle = ColorTheme.transparent(color, 0.3)
            ctx.fillRule = Qt.WindingFill
            ctx.lineWidth = 2

            ctx.moveTo(points[0].x, points[0].y)
            for (var i = 0; i < points.length; ++i)
                ctx.lineTo(points[i].x, points[i].y)
            ctx.lineTo(points[0].x, points[0].y)

            ctx.fill()
            ctx.stroke()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    Repeater
    {
        id: gripRepeater

        model: pointMakerInstrument.model

        PointGrip
        {
            enabled: !pointMakerInstrument.enabled
            color: figure.color
            x: F.absX(model.x, figure)
            y: F.absY(model.y, figure)

            property bool snappingEnabled: pointMakerInstrument.count > 3

            function snapPoints()
            {
                const p1 = pointMakerInstrument.getPoint(index - 1)
                const p2 = pointMakerInstrument.getPoint(index + 1)
                return [p1, p2]
            }

            onMoved:
            {
                if (snappingEnabled)
                {
                    const points = snapPoints()

                    var snapPointIndex = F.findSnapPoint(Qt.point(x, y), points, snapDistance)
                    if (snapPointIndex !== -1)
                    {
                        const snapPoint = points[snapPointIndex]
                        x = snapPoint.x
                        y = snapPoint.y
                    }
                }

                pointMakerInstrument.setPoint(index, Qt.point(x, y))
            }

            onDraggingChanged:
            {
                if (dragging)
                    return

                if (snappingEnabled)
                {
                    const points = snapPoints()

                    var snapPointIndex = F.findSnapPoint(Qt.point(x, y), points, snapDistance)
                    if (snapPointIndex !== -1)
                        pointMakerInstrument.removePoint(index)
                }
            }

            onHoveredChanged:
            {
                if (hovered)
                {
                    lastHoveredGrip = this
                    hoverInstrument.clear()
                }
            }
        }
    }

    PointGrip
    {
        id: temporaryGrip

        visible: hoverInstrument.edgeHovered
        color: figure.color
        ghost: !pressed

        x: (hoverInstrument.hoveredEdgeStart.x + hoverInstrument.hoveredEdgeEnd.x) / 2
        y: (hoverInstrument.hoveredEdgeStart.y + hoverInstrument.hoveredEdgeEnd.y) / 2

        property int pointIndex

        onPressedChanged:
        {
            if (pressed)
            {
                pointIndex = hoverInstrument.hoveredEdgeIndex
                pointMakerInstrument.insertPoint(pointIndex, Qt.point(x, y))
            }
            else
            {
                hoverInstrument.clear()
            }
        }

        onMoved: pointMakerInstrument.setPoint(pointIndex, Qt.point(x, y))
    }

    function refresh()
    {
        pathUtil.points = pointMakerInstrument.getRelativePoints()
        canvas.requestPaint()
    }

    function startCreation()
    {
        pointMakerInstrument.start()
    }

    function deserialize(json)
    {
        if (!json)
        {
            pointMakerInstrument.clear()
            return
        }

        color = json.color
        pointMakerInstrument.setRelativePoints(F.deserializePoints(json.points))
    }

    function serialize()
    {
        return {
            "points": F.serializePoints(pointMakerInstrument.getRelativePoints()),
            "color": color.toString()
        }
    }
}
