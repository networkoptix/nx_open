// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Core 1.0

import "../figure_utils.js" as F

Item
{
    id: figure

    property var figureJson: null

    property color color: "white"
    property var points: []

    readonly property bool hasFigure: points.length > 2

    clip: true

    Canvas
    {
        id: canvas

        anchors.fill: parent

        onPaint:
        {
            let ctx = getContext("2d")
            ctx.reset()

            if (points.length < 3)
                return

            ctx.strokeStyle = color
            ctx.fillStyle = ColorTheme.transparent(color, 0.3)
            ctx.fillRule = Qt.WindingFill
            ctx.lineWidth = 2

            const startX = F.absX(points[0][0], canvas)
            const startY = F.absY(points[0][1], canvas)

            ctx.moveTo(startX, startY)
            for (let i = 0; i < points.length; ++i)
                ctx.lineTo(F.absX(points[i][0], canvas), F.absY(points[i][1], canvas))
            ctx.lineTo(startX, startY)

            ctx.fill()
            ctx.stroke()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    onFigureJsonChanged:
    {
        if (!F.isEmptyObject(figureJson))
        {
            color = figureJson.color
            points = figureJson.points
            if (!points)
                points = []
        }
        else
        {
            points = []
        }

        canvas.requestPaint()
    }
}
