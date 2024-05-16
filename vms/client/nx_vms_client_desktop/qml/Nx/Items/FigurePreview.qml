// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Core 1.0

import "private/figure_utils.js" as F

Item
{
    id: preview

    property var figure: null
    property string figureType
    readonly property bool hasFigure: loader.item && loader.item.hasFigure

    Loader
    {
        id: loader

        anchors.fill: parent

        onLoaded: item.figureJson = Qt.binding(function() { return preview.figure })
    }

    onFigureChanged: updateFigure()
    onFigureTypeChanged: updateFigure()

    function updateFigure()
    {
        if (!figure || F.isEmptyObject(figure))
        {
            loader.source = ""
            return
        }

        if (!figure.color)
            figure.color = ColorTheme.colors.roi.palette[0].toString()

        if (figureType === "line")
            loader.source = "private/FigurePreview/Line.qml"
        else if (figureType === "box")
            loader.source = "private/FigurePreview/Box.qml"
        else if (figureType === "polygon")
            loader.source = "private/FigurePreview/Polygon.qml"
        else if (figureType === "size_constraints")
            loader.source = "private/FigurePreview/SizeConstraints.qml"
        else
            loader.source = ""
    }
}
