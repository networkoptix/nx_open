// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Items

// Current position line, internal time marker and external (header or footer) time marker.
// Must fill the parent of `timeScale`. The line is reparented to `lineParent` (which must have the
// same geometry) so it can be stacked independently of the markers.
Item
{
    id: layer

    required property VerticalTimeScale timeScale

    /** An item to parent the time marker line into. */
    required property Item lineParent

    required property real positionMs
    required property bool positionAtLive
    required property bool windowAtLive

    property bool interactive: true

    // Whether external markers can be pulled in; otherwise they are only tappable.
    property bool pullInEnabled: true
    property alias pullInThreshold: hitArea.pullInThreshold

    property real markerWidth: 80
    property real externalMarkerHeight: 23

    readonly property alias markerHeight: timeMarker.height

    readonly property bool isAhead: positionAtLive
        || (positionMs >= timeScale.startTimeMs + timeScale.durationMs)

    readonly property bool isBehind: positionMs < timeScale.startTimeMs

    readonly property bool isOutside: isAhead || isBehind

    readonly property real lineY: timeScale.y + timeScale.timeToPosition(positionMs)

    // From the press on a draggable marker until release or cancellation.
    readonly property alias positioning: hitArea.positioning

    // From the press on any marker until release or cancellation.
    readonly property alias pressed: hitArea.pressed

    readonly property alias dragScenePosition: hitArea.scenePosition

    signal positionRequested(real timeMs)
    signal scrollRequested(real timeMs)

    Item
    {
        id: scaleArea

        x: layer.timeScale.x
        y: layer.timeScale.y
        width: layer.timeScale.width
        height: layer.timeScale.height
    }

    // A line to mark current position inside of the current timeline window.
    Rectangle
    {
        id: timeMarkerLine

        parent: layer.lineParent

        color: ColorTheme.colors.mobileTimeline.timeMarker.background
        visible: !layer.isOutside

        y: layer.lineY
        width: layer.lineParent.width
        height: 1
    }

    // Time marker to display position inside of the current timeline window.
    TimeMarker
    {
        id: timeMarker

        property real horizontalOffset: layer.positioning ? scaleArea.width : 0
        Behavior on horizontalOffset
        {
            enabled: !hitArea.pullInEnabled //< Shifted at once when pulled in.
            NumberAnimation { duration: 100 }
        }

        readonly property real verticalOffset: //< To avoid extending down out of the layer.
            Math.max(0, layer.lineY + height * 0.5 - layer.height)

        anchors.right: scaleArea.right
        anchors.rightMargin: horizontalOffset
        y: layer.lineY + timeMarkerLine.height * 0.5 - height * 0.5 - verticalOffset
        width: layer.markerWidth

        bottomRoundingRadius: //< Reduce to avoid timeMarkerLine crossing the rounding arc.
            MathUtils.bound(0, height * 0.5 - verticalOffset, topRoundingRadius)

        relevant: !layer.isOutside

        labelFormatter: layer.timeScale.labelFormatter
        timeZone: layer.timeScale.timeZone
        positionMs: layer.positionMs
    }

    // Header or footer time marker to display position outside of the current timeline window.
    ExternalTimeMarker
    {
        id: externalTimeMarker

        anchors.bottom: layer.isAhead ? scaleArea.top : scaleArea.bottom
        anchors.right: scaleArea.right
        anchors.rightMargin: timeMarker.horizontalOffset

        widthAtLive: timeMarker.width
        height: layer.externalMarkerHeight
        topRoundingRadius: timeMarker.topRoundingRadius

        relevant: layer.isOutside

        labelFormatter: layer.timeScale.labelFormatter
        timeZone: layer.timeScale.timeZone
        positionMs: layer.positionAtLive ? -1 : layer.positionMs
    }

    // Interactive area of the currently displayed marker. It stays visible when the marker
    // switches between the internal and the external one, so the drag is not interrupted.
    TimelineDraggable
    {
        id: hitArea

        readonly property Item marker: layer.isOutside ? externalTimeMarker : timeMarker

        x: marker.x
        y: marker.y
        width: marker.width
        height: marker.height

        timeScale: layer.timeScale
        interactive: layer.interactive

        // The external marker displaying Live with the window at live is dragged directly.
        readonly property bool directlyDraggable:
            !layer.isOutside || (layer.positionAtLive && layer.windowAtLive)

        dragEnabled: directlyDraggable || layer.pullInEnabled
        pullInEnabled: !directlyDraggable && layer.pullInEnabled

        onPositioningChanged:
        {
            // The marker moves by the pointer travel, so it doesn't jump when grabbed. The
            // external Live marker follows the pointer.
            if (positioning)
                d.grabOffset = layer.isOutside ? 0 : pressY - (layer.lineY - layer.timeScale.y)
        }

        onDragged: (y) =>
            d.applyDrag(y - d.grabOffset)

        onTapped:
        {
            if (!layer.isOutside)
                return

            layer.scrollRequested(layer.positionAtLive
                ? NxGlobals.syncNowMs()
                : layer.positionMs)
        }
    }

    QtObject
    {
        id: d

        property real grabOffset: 0

        function applyDrag(y)
        {
            // Don't let the time marker go outside of the window, except for Live.
            const minY = layer.windowAtLive ? 0 : 0.5
            const boundedY = MathUtils.bound(minY, y, layer.timeScale.height - 1)

            layer.positionRequested(layer.windowAtLive && boundedY < 0.01
                ? NxGlobals.syncNowMs()
                : layer.timeScale.positionToTime(boundedY))
        }
    }
}
