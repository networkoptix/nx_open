import QtQuick 2.0
import QtQuick.Shapes 1.11
import nx.vms.client.core 1.0

import "../figure_utils.js" as F

Figure
{
    id: figure

    readonly property bool hasFigure: pointMakerInstrument.count === 2
        || (pointMakerInstrument.enabled && pointMakerInstrument.count > 0)

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: enabled
        cursorShape: (dragInstrument.enabled || dragInstrument.dragging)
            ? Qt.SizeAllCursor : Qt.ArrowCursor
    }

    PointMakerInstrument
    {
        id: pointMakerInstrument

        item: mouseArea
        maxPoints: 2
    }

    FigureDragInstrument
    {
        id: dragInstrument
        pointMakerInstrument: pointMakerInstrument
        forcefullyHovered: valid && rectangle.contains(
            Qt.point(mouseArea.mouseX - rectangle.x, mouseArea.mouseY - rectangle.y))
        item: mouseArea
        target: figure
    }

    Rectangle
    {
        id: rectangle

        visible: pointMakerInstrument.count == 2

        border.width: 2
        border.color: figure.color
        color: Qt.rgba(figure.color.r, figure.color.g, figure.color.b, 0.3)

        x: Math.min(pointMakerInstrument.startX, pointMakerInstrument.endX) - 1
        y: Math.min(pointMakerInstrument.startY, pointMakerInstrument.endY) - 1
        width: Math.abs(pointMakerInstrument.endX - pointMakerInstrument.startX) + 2
        height: Math.abs(pointMakerInstrument.endY - pointMakerInstrument.startY) + 2
    }

    readonly property bool valid: pointMakerInstrument.count === 2

    PointGrip
    {
        id: topLeftGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: pointMakerInstrument.count > 0

        x: pointMakerInstrument.startX
        y: pointMakerInstrument.startY

        onXChanged: visible && pointMakerInstrument.setPointX(0, x)
        onYChanged: visible && pointMakerInstrument.setPointY(0, y)
    }

    PointGrip
    {
        id: bottomRightGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: pointMakerInstrument.count > 0

        x: pointMakerInstrument.endX
        y: pointMakerInstrument.endY

        onXChanged: visible && pointMakerInstrument.setPointX(1, x)
        onYChanged: visible && pointMakerInstrument.setPointY(1, y)
    }

    PointGrip
    {
        id: topRightGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
    }
    PointGrip
    {
        id: bottomLeftGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
    }
    PointGrip
    {
        id: leftGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
        axis: Drag.XAxis
        borderColor: "transparent"
        cursorShape: Qt.SizeHorCursor
        y: visible ? (topLeftGrip.y + bottomLeftGrip.y) / 2 : 0
    }
    PointGrip
    {
        id: rightGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
        axis: Drag.XAxis
        borderColor: "transparent"
        cursorShape: Qt.SizeHorCursor
        y: leftGrip.y
    }
    PointGrip
    {
        id: topGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
        axis: Drag.YAxis
        borderColor: "transparent"
        cursorShape: Qt.SizeVerCursor
        x: visible ? (topLeftGrip.x + topRightGrip.x) / 2 : 0
    }
    PointGrip
    {
        id: bottomGrip

        enabled: !pointMakerInstrument.enabled
        color: figure.color
        visible: valid
        axis: Drag.YAxis
        borderColor: "transparent"
        cursorShape: Qt.SizeVerCursor
        x: topGrip.x
    }

    PropertiesSync
    {
        Property { target: leftGrip; property: "x" }
        Property { target: topLeftGrip; property: "x" }
        Property { target: bottomLeftGrip; property: "x" }
        enabled:
        {
            if (valid)
                setValue(pointMakerInstrument.startX)
            return valid
        }
    }
    PropertiesSync
    {
        Property { target: rightGrip; property: "x" }
        Property { target: topRightGrip; property: "x" }
        Property { target: bottomRightGrip; property: "x" }
        enabled:
        {
            if (valid)
                setValue(pointMakerInstrument.endX)
            return valid
        }
    }
    PropertiesSync
    {
        Property { target: topGrip; property: "y" }
        Property { target: topRightGrip; property: "y" }
        Property { target: topLeftGrip; property: "y" }
        enabled:
        {
            if (valid)
                setValue(pointMakerInstrument.startY)
            return valid
        }
    }
    PropertiesSync
    {
        Property { target: bottomGrip; property: "y" }
        Property { target: bottomLeftGrip; property: "y" }
        Property { target: bottomRightGrip; property: "y" }
        enabled:
        {
            if (valid)
                setValue(pointMakerInstrument.endY)
            return valid
        }
    }

    function startCreation()
    {
        pointMakerInstrument.start()
    }

    function deserialize(json)
    {
        if (!json)
        {
            pointMakerInstrument.clear()
            return
        }

        color = json.color
        pointMakerInstrument.setRelativePoints(F.deserializePoints(json.points))
    }

    function serialize()
    {
        return {
            "points": F.serializePoints(pointMakerInstrument.getRelativePoints()),
            "color": color.toString()
        }
    }
}
