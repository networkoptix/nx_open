import QtQuick 2.0
import QtQuick.Controls 2.0
import Nx 1.0

import "private/FigureEditor"

Item
{
    id: editor

    property Figure figure: null

    property string figureType: "polygon"
    readonly property bool hasFigure: figure && figure.hasFigure
    property color color

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.transparent("black", 0.5)
    }

    Loader
    {
        id: figureLoader

        anchors.fill: parent

        source:
        {
            if (figureType === "line")
                return "private/FigureEditor/Line.qml"
            else if (figureType === "box")
                return "private/FigureEditor/Box.qml"
            else if (figureType === "polygon")
                return "private/FigureEditor/Polygon.qml"
            else
                return ""
        }

        onLoaded: figure = item
    }

    Binding
    {
        target: editor
        property: "color"
        value: figure.color
        when: figure
    }

    onColorChanged:
    {
        figure.color = color
    }

    function deserialize(json)
    {
        figure.deserialize(json)
        if (!figure.hasFigure)
            figure.startCreation()
    }

    function serialize()
    {
        return figure.serialize()
    }

    function clear()
    {
        figure.startCreation()
    }
}
