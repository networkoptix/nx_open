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

    onFigureJsonChanged:
    {
        if (figureJson)
        {
            color = figureJson.color
            points = figureJson.points
        }
        else
        {
            points = []
        }
    }
}
