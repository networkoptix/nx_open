// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

import Nx.Core 1.0
import Nx.Core.Animations 1.0

Item
{
    id: grid

    default property alias content: viewport.children

    property alias cellAspectRatio: viewport.cellAspectRatio
    readonly property alias viewScale: viewport.viewScale
    readonly property alias viewCenter: viewport.viewCenter
    readonly property alias oneCellScale: viewport.oneCellScale
    property alias zoomedItem: viewport.zoomedItem
    readonly property alias zoom: d.zoom

    property int zoomAnimationDuration: 1000
    property int stickyZoomAnimationDuration: 3000
    property int zoomOvershootAnimationDuration: 200
    property int returnToBoundsAnimationDuration: 700
    property int fitInViewAnimationDuration: 500

    property real stickyZoomThreshold: 1.5
    property real zoomOvershoot: 1.2

    property real maxItemZoom: 2

    property bool alignToCenter: zoomedItem

    property real margins: 0
    property real topMargin: margins
    property real bottomMargin: margins
    property real leftMargin: margins
    property real rightMargin: margins

    readonly property alias viewport: viewport

    QtObject
    {
        id :d

        property point defaultCenter
        property real defaultScale: 1
        property rect contentBounds
        property Item previousZoomItem

        property point panOffset: Qt.point(0, 0)

        // Unfortunately QSizeF overload does not work.
        property vector2d viewWindowSize:
        {
            viewScale //< Just to trigger re-calculation.
            return viewport.mapFromViewport(Qt.vector2d(viewport.width, viewport.height))
        }

        property real maxPanOffsetX: (alignToCenter && viewWindowSize.x > contentBounds.width)
            ? 0
            : Math.abs(viewWindowSize.x - contentBounds.width) / 2

        property real maxPanOffsetY: (alignToCenter && viewWindowSize.y > contentBounds.height)
            ? 0
            : Math.abs(viewWindowSize.y - contentBounds.height) / 2

        readonly property real maxZoom: oneCellScale / defaultScale * maxItemZoom
        property real zoom: 1
        property point zoomOffset: Qt.point(0, 0)
    }

    GridViewport
    {
        id: viewport

        anchors
        {
            fill: parent
            topMargin: grid.topMargin
            bottomMargin: grid.bottomMargin
            leftMargin: grid.leftMargin
            rightMargin: grid.rightMargin
        }

        onDefaultParametersChanged:
        {
            // Skip animation for initial parameters setting or when a zoomed item changes its
            // geometry.
            const immediately = d.defaultScale === 1
                || (zoomedItem && d.previousZoomItem === zoomedItem)

            const dc = MathUtils.pointSub(d.defaultCenter, defaultParameters.center)
            const dz = d.defaultScale / defaultParameters.scale
            d.defaultCenter = defaultParameters.center
            d.contentBounds = defaultParameters.bounds
            d.defaultScale = defaultParameters.scale
            d.panOffset = MathUtils.pointAdd(d.panOffset, dc)
            d.zoom *= dz

            d.previousZoomItem = zoomedItem

            fitInView(immediately)
        }
    }

    Binding
    {
        target: viewport
        property: "viewCenter"
        value: Qt.point(
            d.defaultCenter.x + d.panOffset.x + d.zoomOffset.x,
            d.defaultCenter.y + d.panOffset.y + d.zoomOffset.y)
    }

    Binding
    {
        target: viewport
        property: "viewScale"
        value: d.defaultScale * d.zoom
    }

    PropertyAnimation
    {
        id: boundsAnimation

        property point point

        target: this
        property: "point"
        easing.type: Easing.OutCubic

        onPointChanged: d.panOffset = point

        function restart(point, duration)
        {
            stop()

            from = d.panOffset
            to = point
            this.duration = duration

            start()
        }
    }

    VelocityMeter { id: velocityMeter }
    MomentumAnimation
    {
        id: panOffsetInetrtiaAnimation

        property point point
        target: panOffsetInetrtiaAnimation
        property: "point"

        property point originalPanOffset
        property real startScale

        onPointChanged:
        {
            const multipler = viewScale / startScale
            d.panOffset = MathUtils.pointAdd(
                originalPanOffset, Qt.point(point.x * multipler, point.y * multipler))
        }

        maximumVelocity: 2 / (viewScale / d.defaultScale)
        deceleration: 10 / (viewScale / d.defaultScale)

        onAboutToStart:
        {
            originalPanOffset = d.panOffset
            startScale = viewScale
            point = Qt.point(0, 0)
        }

        onStopped: returnToBounds()
    }

    PointHandler
    {
        id: pointHandler

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property point pressPanOffset

        onActiveChanged:
        {
            if (active)
            {
                panOffsetInetrtiaAnimation.stop()
                boundsAnimation.stop()

                pressPanOffset = d.panOffset
                velocityMeter.start(pressPanOffset)
            }
            else
            {
                pan()

                if (d.panOffset === pressPanOffset)
                    return

                panOffsetInetrtiaAnimation.restart(velocityMeter.velocity)
            }
        }

        onPointChanged:
        {
            if (!active)
                return

            pan()
            velocityMeter.addPoint(d.panOffset)
        }

        function pan()
        {
            d.panOffset = MathUtils.pointSub(pressPanOffset, calculateTranslation())
        }

        function calculateTranslation()
        {
            return viewport.mapFromViewport(Qt.vector2d(
                point.position.x - point.pressPosition.x,
                point.position.y - point.pressPosition.y))
        }
    }

    NumberAnimation
    {
        id: zoomAnimation

        target: zoomAnimation
        property: "zoom"

        property real zoom: 1
        property point origin
        property bool blockOffsets: false

        property bool restarting: false

        duration: 1000
        easing.type: Easing.OutCubic

        onZoomChanged:
        {
            if (blockOffsets)
            {
                d.zoom = zoom
                return
            }

            const visualOrigin = viewport.mapToViewport(origin)
            d.zoom = zoom
            const newVisualOrigin = viewport.mapToViewport(origin)

            const shift = MathUtils.pointSub(newVisualOrigin, visualOrigin)
            const centerShift = viewport.mapFromViewport(Qt.vector2d(shift.x, shift.y))

            d.zoomOffset = MathUtils.pointAdd(d.zoomOffset, centerShift)
        }

        function restart(origin, targetZoom, duration, blockOffsets)
        {
            restarting = true

            stop()

            from = d.zoom
            to = targetZoom
            this.duration = duration
            this.origin = origin
            this.blockOffsets = !!blockOffsets
            stickyZoomActivationTimer.stop()

            start()

            restarting = false
        }

        onStopped:
        {
            if (pointHandler.active || restarting)
                return

            if (d.zoom < 1 || d.zoom > d.maxZoom)
            {
                const targetZoom = MathUtils.bound(1, d.zoom, d.maxZoom)
                zoomAnimation.restart(origin, targetZoom, returnToBoundsAnimationDuration)
            }
            else
            {
                stickyZoomActivationTimer.restart()
            }
        }
    }

    WheelHandler
    {
        id: wheelHandler

        onRotationChanged:
        {
            if (!active || rotation === 0)
                return

            const origin = viewport.mapFromViewport(point.position)
            const multipler = Math.pow(1.1, rotation / 15)
            let targetZoom =
                multipler * (zoomAnimation.running ? zoomAnimation.to : d.zoom);

            targetZoom = MathUtils.bound(1 / zoomOvershoot, targetZoom, d.maxZoom * zoomOvershoot)
            const overshoot = targetZoom < 1 || targetZoom > d.maxZoom

            zoomAnimation.restart(origin, targetZoom,
                overshoot ? zoomOvershootAnimationDuration : zoomAnimationDuration)

            rotation = 0
        }
    }

    TapHandler
    {
        onDoubleTapped: fitInView()
    }

    Timer
    {
        id: stickyZoomActivationTimer

        interval: 200
        onTriggered:
        {
            if (d.zoom < stickyZoomThreshold)
                zoomAnimation.restart(zoomAnimation.origin, 1, stickyZoomAnimationDuration)
        }
    }

    function clearZoomOffset()
    {
        d.panOffset = MathUtils.pointAdd(d.panOffset, d.zoomOffset)
        pointHandler.pressPanOffset = MathUtils.pointAdd(pointHandler.pressPanOffset, d.zoomOffset)
        d.zoomOffset = Qt.point(0, 0)
    }

    function stopAnimations()
    {
        panOffsetInetrtiaAnimation.stop()
        zoomAnimation.stop()
        boundsAnimation.stop()
        stickyZoomActivationTimer.stop()
    }

    function fitInView(immediately)
    {
        stopAnimations()
        clearZoomOffset()

        if (immediately)
        {
            d.panOffset = Qt.point(0, 0)
            pointHandler.pressPanOffset = Qt.point(0, 0)
            d.zoomOffset = Qt.point(0, 0)
            d.zoom = 1
        }
        else
        {
            zoomAnimation.restart(d.defaultCenter, 1, fitInViewAnimationDuration, true)
            boundsAnimation.restart(Qt.point(0, 0), fitInViewAnimationDuration)
        }
    }

    function returnToBounds(immediately)
    {
        stopAnimations()
        clearZoomOffset()

        const p = Qt.point(
            MathUtils.bound(-d.maxPanOffsetX, d.panOffset.x, d.maxPanOffsetX),
            MathUtils.bound(-d.maxPanOffsetY, d.panOffset.y, d.maxPanOffsetY))

        if (immediately)
            d.panOffset = p
        else
            boundsAnimation.restart(p, returnToBoundsAnimationDuration)
    }
}
