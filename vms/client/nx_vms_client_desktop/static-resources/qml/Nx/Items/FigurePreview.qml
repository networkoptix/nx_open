import QtQuick 2.0

Item
{
    id: preview

    property var figure: null
    readonly property bool hasFigure: loader.item && loader.item.hasFigure

    Loader
    {
        id: loader

        anchors.fill: parent

        onLoaded: item.figureJson = Qt.binding(function() { return preview.figure })
    }

    onFigureChanged:
    {
        if (!figure)
        {
            loader.source = ""
            return
        }

        if (figure.type === "line")
            loader.source = "private/FigurePreview/Line.qml"
        else if (figure.type === "box")
            loader.source = "private/FigurePreview/Box.qml"
        else if (figure.type === "polygon")
            loader.source = "private/FigurePreview/Polygon.qml"
        else
            loader.source = ""
    }
}
