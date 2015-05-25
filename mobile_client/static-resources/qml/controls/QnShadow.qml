import QtQuick 2.2
import QtGraphicalEffects 1.0

import "../common_functions.js" as Common

DropShadow {
    id: shadow

    property real offsetLow: 1.5
    property real offsetHigh: 8
    property real radiusLow: 1.5
    property real radiusHigh: 19
    property real opacityLow: 0.12
    property real opacityHigh: 0.30
    property real zdepth: 1

    cached: true
    samples: Math.max(32, Math.round(radius * 2))

    spread: 0
    horizontalOffset: 0
    verticalOffset: Common.dp(Common.interpolate(offsetLow, offsetHigh, Common.perc(1, 5, Math.min(zdepth, 5))))
    radius: Common.dp(Common.interpolate(radiusLow, radiusHigh, Common.perc(1, 5, Math.min(zdepth, 5))))
    color: Qt.rgba(0, 0, 0, Common.interpolate(opacityLow, opacityHigh, Common.perc(1, 5, Math.min(zdepth, 5))))
}
