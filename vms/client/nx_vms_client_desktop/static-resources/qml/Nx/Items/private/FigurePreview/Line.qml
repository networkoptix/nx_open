import QtQuick 2.0
import Nx 1.0
import Nx.Items 1.0 as Items

import "../figure_utils.js" as F

Item
{
    id: figure

    property var figureJson: null

    property color color: "white"
    property var points: []
    property string direction: ""

    readonly property bool hasFigure: points.length === 2

    clip: true

    Items.Line
    {
        id: line

        visible: hasFigure
        color: figure.color
        lineWidth: 2

        startX: points.length === 2 ? F.absX(points[0][0], figure) : 0
        startY: points.length === 2 ? F.absY(points[0][1], figure) : 0
        endX: points.length === 2 ? F.absX(points[1][0], figure) : 0
        endY: points.length === 2 ? F.absY(points[1][1], figure) : 0
    }

    LineArrow
    {
        x1: line.startX
        y1: line.startY
        x2: line.endX
        y2: line.endY
        color: figure.color
        visible: !direction || direction === "a"
    }

    LineArrow
    {
        x1: line.endX
        y1: line.endY
        x2: line.startX
        y2: line.startY
        color: figure.color
        visible: !direction || direction === "b"
    }

    onFigureJsonChanged:
    {
        if (!F.isEmptyObject(figureJson))
        {
            color = figureJson.color
            points = figureJson.points
            direction = figureJson.direction || ""
            if (!points)
                points = []
        }
        else
        {
            points = []
        }
    }
}
