import QtQuick 2.10
import Nx.Controls 1.0

import "../figure_utils.js" as F

PolyFigure
{
    id: figure

    property string allowedDirections: (figureSettings && figureSettings.allowedDirections)
        ? figureSettings.allowedDirections : ""

    canvas.opacity: (arrowLeft.hovered || arrowRight.hovered) ? 0.5 : 1.0

    LineArrow
    {
        id: arrowLeft

        visible: !pointMakerInstrument.enabled && allowedDirections !== "none"
            && pathUtil.length > 0

        color: figure.color

        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle)

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowRight.checked = !checked
            }
            else if (allowedDirections !== "none" && !checked && !arrowRight.checked)
            {
                arrowRight.checked = true
            }
        }
    }

    LineArrow
    {
        id: arrowRight

        visible: !pointMakerInstrument.enabled && allowedDirections !== "none"
            && pathUtil.length > 0

        color: figure.color

        x: pathUtil.midAnchorPoint.x
        y: pathUtil.midAnchorPoint.y
        rotation: F.toDegrees(pathUtil.midAnchorPointNormalAngle) + 180

        onToggled:
        {
            if (allowedDirections === "one")
            {
                arrowLeft.checked = !checked
            }
            else if (allowedDirections !== "none" && !checked && !arrowLeft.checked)
            {
                arrowLeft.checked = true
            }
        }
    }

    hint:
    {
        if (pointMakerInstrument.enabled)
        {
            if (pointMakerInstrument.count === 0)
                return qsTr("Click on video to start line.")
            if (minPoints > geometricMinPoints && pointMakerInstrument.count < minPoints + 1)
                return qsTr("This line requires at least %n points.", "", minPoints)
        }
        else
        {
            if (pathUtil.hasSelfIntersections)
                return qsTr("Line is not valid. Remove self-intersections to proceed.")

            if (pointMakerInstrument.count < minPoints)
                return qsTr("This line requires at least %n points.", "", minPoints)

            if (pointMakerInstrument.count === maxPoints && hoverInstrument.edgeHovered)
                return qsTr("Maximum points count is reached (%n points).", "", maxPoints)

            if (allowedDirections !== "none")
                return qsTr("Click arrows to toggle the desired directions.")
        }

        return ""
    }
    hintStyle:
    {
        if (!pointMakerInstrument.enabled
             && (pathUtil.hasSelfIntersections || pointMakerInstrument.count < minPoints))
        {
            return Banner.Style.Error
        }
        return Banner.Style.Info
    }

    function startCreation()
    {
        pointMakerInstrument.start()

        arrowLeft.checked = true
        arrowRight.checked = (allowedDirections !== "one")
    }

    function deserialize(json)
    {
        baseDeserialize(json)

        if (!hasFigure)
            return

        if (json.direction === "left")
        {
            arrowLeft.checked = true
            arrowRight.checked = false
        }
        else if (json.direction === "right")
        {
            arrowLeft.checked = false
            arrowRight.checked = true
        }
        else
        {
            arrowLeft.checked = true
            arrowRight.checked = true
        }
    }

    function serialize()
    {
        var json = baseSerialize()
        if (!json)
            return null

        json.direction = (allowedDirections !== "none" && arrowLeft.checked !== arrowRight.checked)
            ? arrowLeft.checked ? "left" : "right"
            : "absent"

        return json
    }
}
