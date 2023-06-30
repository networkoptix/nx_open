// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.10
import QtQml 2.3
import nx.vms.client.core 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Qt.labs.platform 1.0

import "../figure_utils.js" as F

Figure
{
    id: figure

    readonly property int geometricMinPoints: polygon ? 3 : 2
    readonly property int minPoints:
        (figureSettings && figureSettings.minPoints > geometricMinPoints)
            ? figureSettings.minPoints
            : geometricMinPoints
    readonly property int maxPoints:
        (figureSettings && figureSettings.maxPoints && figureSettings.maxPoints >= minPoints)
            ? figureSettings.maxPoints
            : polygon ? -1 : 2
    property real snapDistance: 8
    readonly property bool hasFigure: pointsCount >= geometricMinPoints
        || (pointMakerInstrument.enabled && pointsCount > 0)
    acceptable: !pathUtil.hasSelfIntersections && pointsCount >= minPoints
        && (!pointMakerInstrument.enabled || pointsCount === 0)

    property bool polygon: false

    readonly property alias pointsCount: pointMakerInstrument.count

    property bool gripDragging: false

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: enabled
        cursorShape: (dragInstrument.enabled || dragInstrument.dragging)
            ? Qt.SizeAllCursor : Qt.ArrowCursor
    }

    readonly property var pointMakerInstrument: PointMakerInstrument
    {
        id: pointMakerInstrument

        minPoints: figure.minPoints
        maxPoints: figure.maxPoints

        enabled: false
        closed: polygon

        item: mouseArea

        onEnabledChanged: canvas.requestPaint()
    }

    PathHoverInstrument
    {
        id: hoverInstrument
        model: pointMakerInstrument.model
        enabled: !pointMakerInstrument.enabled
        item: mouseArea
        closed: polygon
    }

    hoverInstrument: hoverInstrument.enabled ? hoverInstrument : pointMakerInstrument

    FigureDragInstrument
    {
        id: dragInstrument
        pointMakerInstrument: pointMakerInstrument
        pathHoverInstrument: hoverInstrument
        forcefullyHovered: polygon && pathUtil.contains(
            Qt.point(mouseArea.mouseX, mouseArea.mouseY))
        item: mouseArea
        target: figure
        minX: -pathUtil.boundingRect.x
        minY: -pathUtil.boundingRect.y
        maxX: width - (pathUtil.boundingRect.x + pathUtil.boundingRect.width)
        maxY: height - (pathUtil.boundingRect.y + pathUtil.boundingRect.height)
    }

    readonly property var pathUtil: PathUtil
    {
        id: pathUtil

        closed: polygon

        property bool hasSelfIntersections: false
    }

    Connections
    {
        target: pointMakerInstrument.model
        function onDataChanged() { refresh() }
        function onRowsInserted() { refresh() }
        function onRowsRemoved() { refresh() }
        function onModelReset() { refresh() }
    }

    property PointGrip lastHoveredGrip: null

    readonly property Canvas canvas: Canvas
    {
        id: canvas

        // Visual item created in a property has an invalid visual parent.
        parent: figure
        anchors.fill: parent

        onPaint:
        {
            const points = pointMakerInstrument.getPoints()

            let ctx = getContext("2d")
            ctx.reset()

            if (points.length < 2)
                return

            ctx.strokeStyle = color
            ctx.fillStyle = ColorTheme.transparent(color, 0.3)
            ctx.fillRule = Qt.WindingFill
            ctx.lineWidth = 2
            ctx.lineJoin = "bevel"

            ctx.moveTo(points[0].x, points[0].y)
            for (let i = 0; i < points.length; ++i)
                ctx.lineTo(points[i].x, points[i].y)

            if (polygon && points.length > 2 && !pointMakerInstrument.enabled)
            {
                ctx.lineTo(points[0].x, points[0].y)
                ctx.fill()
            }

            ctx.stroke()
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
            z: dragging ? 10 : 0

            property bool snappingEnabled: true

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

                    const snapPointIndex = F.findSnapPoint(Qt.point(x, y), points, snapDistance)
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
                gripDragging = dragging

                if (dragging)
                    return

                if (snappingEnabled)
                {
                    const points = snapPoints()

                    const snapPointIndex = F.findSnapPoint(Qt.point(x, y), points, snapDistance)
                    if (snapPointIndex !== -1)
                    {
                        if (pointsCount > 3)
                            pointMakerInstrument.removePoint(index)
                        else
                            startCreation()
                    }
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

            onClicked:
            {
                if (button === Qt.RightButton)
                {
                    pointMenu.pointIndex = index
                    pointMenu.open()
                }
            }
        }
    }

    PointGrip
    {
        id: temporaryGrip

        visible: hoverInstrument.edgeHovered
            && (maxPoints <= 0 || pointMakerInstrument.count < maxPoints)
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

        onDraggingChanged: gripDragging = dragging
    }

    PointMarker
    {
        color: figure.color
        x: pointMakerInstrument.hoveredPoint ? pointMakerInstrument.hoveredPoint.x : 0
        y: pointMakerInstrument.hoveredPoint ? pointMakerInstrument.hoveredPoint.y : 0
        hovered: true
        visible: !!pointMakerInstrument.hoveredPoint
    }

    Menu
    {
        id: pointMenu

        property int pointIndex: -1

        MenuItem
        {
            text: qsTr("Delete")
            onTriggered:
            {
                if (pointMenu.pointIndex < 0)
                    return

                if (pointMakerInstrument.count > geometricMinPoints)
                    pointMakerInstrument.removePoint(pointMenu.pointIndex)
                else
                    startCreation()

            }
        }

        onVisibleChanged:
        {
            if (!visible)
                pointMenu.pointIndex = -1
        }
    }

    onColorChanged: canvas.requestPaint()

    onWidthChanged: refresh()
    onHeightChanged: refresh()

    function refresh()
    {
        pathUtil.points = pointMakerInstrument.getPoints()
        pathUtil.hasSelfIntersections = pathUtil.checkSelfIntersections()
        canvas.requestPaint()
    }

    function startCreation()
    {
        pointMakerInstrument.start()
    }

    function baseDeserialize(json)
    {
        pointMakerInstrument.finish()

        if (F.isEmptyObject(json))
        {
            pointMakerInstrument.clear()
            return
        }

        color = json.color || ""
        pointMakerInstrument.setRelativePoints(F.deserializePoints(json.points || []))
    }

    function baseSerialize()
    {
        if (pointMakerInstrument.count === 0)
            return null

        return {
            "points": F.serializePoints(pointMakerInstrument.getRelativePoints()),
            "color": color.toString()
        }
    }
}
