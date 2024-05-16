// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Core 1.0
import Nx.Items 1.0 as Items
import nx.vms.client.core 1.0

import "../figure_utils.js" as F

Item
{
    id: figure

    property var figureJson: null

    property color color: "white"
    property var points: []
    property string direction: ""

    readonly property bool hasFigure: points.length >= 2

    clip: true

    PathUtil
    {
        id: pathUtil
    }

    Canvas
    {
        id: canvas

        anchors.fill: parent

        onPaint:
        {
            let ctx = getContext("2d")
            ctx.reset()

            const points = pathUtil.points
            if (points.length < 2)
                return

            ctx.strokeStyle = color
            ctx.fillStyle = ColorTheme.transparent(color, 0.3)
            ctx.fillRule = Qt.WindingFill
            ctx.lineWidth = 2

            ctx.moveTo(points[0].x, points[0].y)
            for (let i = 0; i < points.length; ++i)
                ctx.lineTo(points[i].x, points[i].y)

            ctx.stroke()
        }
    }

    LineArrow
    {
        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle)
        color: "white"
        visible: !direction || direction === "left" || direction === "absent"
    }

    LineArrow
    {
        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle) + 180
        color: "white"
        visible: !direction || direction === "right" || direction === "absent"
    }

    onFigureJsonChanged:
    {
        if (!F.isEmptyObject(figureJson))
        {
            color = figureJson.color
            points = figureJson.points
            direction = figureJson.direction || ""
            if (!points)
            {
                points = []
            }
            else
            {
                let absPoints = []
                for (let i = 0; i < points.length; ++i)
                {
                    absPoints.push(
                        Qt.point(F.absX(points[i][0], figure), F.absY(points[i][1], figure)))
                }
                pathUtil.points = absPoints
            }
        }
        else
        {
            points = []
        }
    }

    onPointsChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    onWidthChanged: updatePoints()
    onHeightChanged: updatePoints()

    function updatePoints()
    {
        let absPoints = []
        for (let i = 0; i < points.length; ++i)
        {
            absPoints.push(
                Qt.point(F.absX(points[i][0], figure), F.absY(points[i][1], figure)))
        }
        pathUtil.points = absPoints
        canvas.requestPaint()
    }
}
