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

    // This property is a dirty workaround for lack of silent hover events monitoring. In Qt 5.11
    // it is impossible to listen hover events in one item and at the same time receive them in
    // another item behind it. In Qt 5.12 it is possible with HoverHandler. But currently we use
    // Qt 5.11, so have to use a workaround. This property just exposes an active instrument
    // which could be used to check hover events.
    // TODO: #4.2 #dklychkov Remove this property and make FigureEditorDialog banner use
    // HoverHandler.
    property var hoverInstrument: null

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

            editor.hoverInstrument = Qt.binding(function() { return figure.hoverInstrument })
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
