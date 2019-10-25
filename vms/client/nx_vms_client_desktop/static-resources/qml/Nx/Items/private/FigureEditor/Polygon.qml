import QtQuick 2.10
import QtQuick.Shapes 1.11
import QtQml 2.3
import nx.vms.client.core 1.0

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

        onEnabledChanged: pathInstantiator.updateShape()
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
        forcefullyHovered: shape.contains(Qt.point(mouseArea.mouseX, mouseArea.mouseY))
        item: mouseArea
        target: figure
    }

    property PointGrip lastHoveredGrip: null

    Shape
    {
        id: shape

        anchors.fill: parent
//        layer.enabled: true
//        layer.samples: 4
        containsMode: Shape.FillContains

        ShapePath
        {
            id: shapePath

            strokeWidth: 2
            strokeColor: figure.color
            fillColor: pointMakerInstrument.enabled
               ? "transparent"
               : Qt.rgba(strokeColor.r, strokeColor.g, strokeColor.b, 0.3)
            fillRule: ShapePath.WindingFill

            startX: pointMakerInstrument.startX
            startY: pointMakerInstrument.startY
        }
    }

    PathLine
    {
        id: lastLine
        x: pointMakerInstrument.startX
        y: pointMakerInstrument.startY
    }

    Instantiator
    {
        id: pathInstantiator

        model: pointMakerInstrument.model

        PathLine
        {
            x: F.absX(model.x, figure)
            y: F.absY(model.y, figure)
        }

        onObjectAdded: updateShape()
        onObjectRemoved: updateShape()

        function updateShape()
        {
            var elements = []

            for (var i = 1; i < count; ++i)
                elements.push(objectAt(i))

            if (!pointMakerInstrument.enabled)
                elements.push(lastLine)

            shapePath.pathElements = elements
        }
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
