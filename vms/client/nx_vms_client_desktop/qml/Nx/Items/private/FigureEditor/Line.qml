// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
            if (pointMakerInstrument.count > 0)
            {
                const minPoints_ = Math.max(minPoints, geometricMinPoints)
                if (pointMakerInstrument.count < minPoints_ + 1)
                    return qsTr("This line requires at least %n points.", "", minPoints_)
                else
                    return qsTr("Click on the last point to finish drawing the line.")
            }
        }
        else
        {
            if (pathUtil.hasSelfIntersections)
                return qsTr("Line is not valid. Remove self-intersections to proceed.")

            if (pointMakerInstrument.count < minPoints)
                return qsTr("This line requires at least %n points.", "", minPoints)

            if (pointMakerInstrument.count === maxPoints && hoverInstrument.edgeHovered)
                return qsTr("The maximum number of points has been reached (%n points).", "", maxPoints)

            if (allowedDirections !== "none")
                return qsTr("Click on each arrow to toggle the desired direction.")
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
        let json = baseSerialize()
        if (!json)
            return null

        json.direction = (allowedDirections !== "none" && arrowLeft.checked !== arrowRight.checked)
            ? arrowLeft.checked ? "left" : "right"
            : "absent"

        return json
    }
}
