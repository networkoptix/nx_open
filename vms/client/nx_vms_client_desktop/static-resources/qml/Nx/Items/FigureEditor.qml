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
    readonly property bool figureAcceptable: !figure || figure.acceptable
    property color color

    property int hintStyle
    property string hint: ""

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

        onLoaded:
        {
            figure = item
            editor.color = Qt.binding(function() { return figure.color })
            editor.hint = Qt.binding(function() { return figure.hint })
            editor.hintStyle = Qt.binding(function() { return figure.hintStyle })
        }
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
