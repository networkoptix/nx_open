import QtQuick 2.10

import "../figure_utils.js" as F

Figure
{
    id: figure

    readonly property bool hasFigure: pointMakerInstrument.count === 2
        || (pointMakerInstrument.enabled && pointMakerInstrument.count > 0)

    property string allowedDirections: "any"
    acceptable: !pointMakerInstrument.enabled || pointMakerInstrument.count === 0

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

    PathHoverInstrument
    {
        id: hoverInstrument
        model: pointMakerInstrument.model
        enabled: !pointMakerInstrument.enabled
        item: mouseArea
    }

    hoverInstrument: hoverInstrument.enabled ? hoverInstrument : pointMakerInstrument

    FigureDragInstrument
    {
        id: dragInstrument
        pointMakerInstrument: pointMakerInstrument
        pathHoverInstrument: hoverInstrument
        item: mouseArea
        target: figure
        minX: -Math.min(pointMakerInstrument.startX, pointMakerInstrument.endX)
        minY: -Math.min(pointMakerInstrument.startY, pointMakerInstrument.endY)
        maxX: width - Math.max(pointMakerInstrument.startX, pointMakerInstrument.endX)
        maxY: height - Math.max(pointMakerInstrument.startY, pointMakerInstrument.endY)
    }

    Rectangle
    {
        color: figure.color

        antialiasing: true

        x: pointMakerInstrument.startX
        y: pointMakerInstrument.startY - height / 2
        width: F.distance(pointMakerInstrument.startX, pointMakerInstrument.startY,
            pointMakerInstrument.endX, pointMakerInstrument.endY)
        height: 2

        transformOrigin: Item.Left
        rotation: Math.atan2(
            pointMakerInstrument.endY - pointMakerInstrument.startY,
            pointMakerInstrument.endX - pointMakerInstrument.startX) / Math.PI * 180
    }

    Repeater
    {
        model: pointMakerInstrument.model

        PointGrip
        {
            enabled: !pointMakerInstrument.enabled
            color: figure.color
            x: F.absX(model.x, figure)
            y: F.absY(model.y, figure)

            onMoved: pointMakerInstrument.setPoint(index, Qt.point(x, y))
        }
    }

    LineArrow
    {
        id: arrowA

        visible: !pointMakerInstrument.enabled && allowedDirections !== "none"
        color: figure.color

        x1: pointMakerInstrument.startX
        y1: pointMakerInstrument.startY
        x2: pointMakerInstrument.endX
        y2: pointMakerInstrument.endY

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowB.checked = !checked
            }
            else if (allowedDirections === "any" && !checked && !arrowB.checked)
            {
                arrowB.checked = true
            }
        }
    }

    LineArrow
    {
        id: arrowB

        visible: !pointMakerInstrument.enabled && allowedDirections !== "none"
        color: figure.color

        x1: pointMakerInstrument.endX
        y1: pointMakerInstrument.endY
        x2: pointMakerInstrument.startX
        y2: pointMakerInstrument.startY

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowA.checked = !checked
            }
            else if (allowedDirections === "any" && !checked && !arrowA.checked)
            {
                arrowA.checked = true
            }
        }
    }

    hint:
    {
        if (pointMakerInstrument.enabled)
        {
            if (pointMakerInstrument.count === 0)
                return qsTr("Click on video to start line.")
        }
        else
        {
            if (allowedDirections !== "none")
                return qsTr("Click arrows to toggle the desired directions.")
        }

        return ""
    }

    function startCreation()
    {
        pointMakerInstrument.start()

        arrowA.checked = true
        arrowB.checked = (allowedDirections !== "one")
    }

    function deserialize(json)
    {
        pointMakerInstrument.finish()

        if (!json)
        {
            pointMakerInstrument.clear()
            return
        }

        color = json.color
        pointMakerInstrument.setRelativePoints(F.deserializePoints(json.points))

        if (json.direction === "a")
        {
            arrowA.checked = true
            arrowB.checked = false
        }
        else if (json.direction === "b")
        {
            arrowA.checked = false
            arrowB.checked = true
        }
        else
        {
            arrowA.checked = true
            arrowB.checked = true
        }
    }

    function serialize()
    {
        const direction = (arrowA.checked !== arrowB.checked)
            ? arrowA.checked ? "a" : "b"
            : ""

        return {
            "points": F.serializePoints(pointMakerInstrument.getRelativePoints()),
            "direction": direction,
            "color": color.toString()
        }
    }
}
