import QtQuick 2.10
import QtQml 2.3
import nx.vms.client.core 1.0
import Nx 1.0
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
            if (pointMakerInstrument.count === 0)
                return qsTr("Click on video to start polygon.")
        }
        else
        {
            if (pathUtil.hasSelfIntersections)
                return qsTr("Polygon is not valid. Remove self-intersections to proceed.")

            if (pointMakerInstrument.count === maxPoints && hoverInstrument.edgeHovered)
                return qsTr("Maximum points count is reached (%n points).", "", maxPoints)
        }

        return ""
    }
    hintStyle:
    {
        if (!pointMakerInstrument.enabled && pathUtil.hasSelfIntersections)
            return Banner.Style.Error
        return Banner.Style.Info
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
