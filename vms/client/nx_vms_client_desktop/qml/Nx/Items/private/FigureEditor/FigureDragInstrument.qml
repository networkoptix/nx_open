// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Instruments 1.0

DragInstrument
{
    property PointMakerInstrument pointMakerInstrument: null
    property PathHoverInstrument pathHoverInstrument: null

    property bool forcefullyHovered: false

    enabled:
    {
        if (pointMakerInstrument && pointMakerInstrument.enabled)
            return false

        return (pathHoverInstrument && pathHoverInstrument.edgeHovered) || forcefullyHovered
    }

    onFinished:
    {
        const dx = target.x
        const dy = target.y

        let points = pointMakerInstrument.getPoints()
        for (let i = 0; i < pointMakerInstrument.count; ++i)
        {
            let p = points[i]
            p.x += dx
            p.y += dy
            points[i] = p
        }
        pointMakerInstrument.setPoints(points)

        target.x = 0
        target.y = 0
    }
}
