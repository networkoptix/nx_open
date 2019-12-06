import QtQuick 2.0

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
        if (!figure)
        {
            loader.source = ""
            return
        }

        if (figureType === "line")
            loader.source = "private/FigurePreview/Line.qml"
        else if (figureType === "box")
            loader.source = "private/FigurePreview/Box.qml"
        else if (figureType === "polygon")
            loader.source = "private/FigurePreview/Polygon.qml"
        else
            loader.source = ""
    }
}
