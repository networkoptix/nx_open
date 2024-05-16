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

    readonly property bool hasFigure: points.length === 2

    clip: true

    Rectangle
    {
        id: line

        visible: hasFigure
        color: ColorTheme.transparent(figure.color, 0.3)
        border.width: 2
        border.color: figure.color

        property real startX: points.length === 2 ? Math.min(points[0][0], points[1][0]) : 0
        property real startY: points.length === 2 ? Math.min(points[0][1], points[1][1]) : 0
        property real endX: points.length === 2 ? Math.max(points[0][0], points[1][0]) : 0
        property real endY: points.length === 2 ? Math.max(points[0][1], points[1][1]) : 0

        x: F.absX(startX, figure)
        y: F.absY(startY, figure)
        width: F.absX(endX, figure) - x
        height: F.absY(endY, figure) - y
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
    }
}
