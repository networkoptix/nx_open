// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.10
import QtQml 2.3
import nx.vms.client.core 1.0
import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Qt.labs.platform 1.0

import "../figure_utils.js" as F

PolyFigure
{
    id: figure

    polygon: true

    property PointGrip lastHoveredGrip: null

    hint:
    {
        if (pointMakerInstrument.enabled)
        {
            if (pointMakerInstrument.count > 0)
            {
                const minPoints_ = Math.max(minPoints, geometricMinPoints)
                if (pointMakerInstrument.count < minPoints_ + 1)
                    return qsTr("This polygon requires at least %n points.", "", minPoints_)
                else if (pointMakerInstrument.count > minPoints)
                    return qsTr("Click on the last point to finish drawing the polygon.")
            }
        }
        else
        {
            if (pathUtil.hasSelfIntersections)
                return qsTr("Polygon is not valid. Remove self-intersections to proceed.")

            if (pointMakerInstrument.count < minPoints)
                return qsTr("This polygon requires at least %n points.", "", minPoints)

            if (pointMakerInstrument.count === maxPoints && hoverInstrument.edgeHovered)
                return qsTr("The maximum number of points has been reached (%n points).", "", maxPoints)
        }

        return ""
    }
    hintStyle:
    {
        if (!pointMakerInstrument.enabled
             && (pathUtil.hasSelfIntersections || pointMakerInstrument.count < minPoints))
        {
            return DialogBanner.Style.Error
        }
        return DialogBanner.Style.Info
    }

    function deserialize(json)
    {
        baseDeserialize(json)
    }

    function serialize()
    {
        return baseSerialize()
    }
}
