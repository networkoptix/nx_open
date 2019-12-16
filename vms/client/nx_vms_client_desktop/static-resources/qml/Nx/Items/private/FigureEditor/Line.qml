import QtQuick 2.10
import Nx.Controls 1.0

import "../figure_utils.js" as F

PolyFigure
{
    id: figure

    property string allowedDirections: (figureSettings && figureSettings.allowedDirections)
        ? figureSettings.allowedDirections : ""

    LineArrow
    {
        id: arrowA

        visible: !pointMakerInstrument.enabled && allowedDirections !== "none"
        color: figure.color

        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle)

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowB.checked = !checked
            }
            else if (allowedDirections !== "none" && !checked && !arrowB.checked)
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

        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle) + 180

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowA.checked = !checked
            }
            else if (allowedDirections !== "none" && !checked && !arrowA.checked)
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
            if (pathUtil.hasSelfIntersections)
                return qsTr("Line is not valid. Remove self-intersections to proceed.")

            if (pointMakerInstrument.count === maxPoints && hoverInstrument.edgeHovered)
                return qsTr("Maximum points count is reached (%n points).", "", maxPoints)

            if (allowedDirections !== "none")
                return qsTr("Click arrows to toggle the desired directions.")
        }

        return ""
    }
    hintStyle:
    {
        if (!pointMakerInstrument.enabled && pathUtil.hasSelfIntersections)
            return Banner.Style.Error
        return Banner.Style.Info
    }

    function startCreation()
    {
        pointMakerInstrument.start()

        arrowA.checked = true
        arrowB.checked = (allowedDirections !== "one")
    }

    function deserialize(json)
    {
        baseDeserialize(json)

        if (!hasFigure)
            return

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
        var json = baseSerialize()
        if (!json)
            return null

        json.direction = (allowedDirections !== "none" && arrowA.checked !== arrowB.checked)
            ? arrowA.checked ? "a" : "b"
            : ""

        return json
    }
}
