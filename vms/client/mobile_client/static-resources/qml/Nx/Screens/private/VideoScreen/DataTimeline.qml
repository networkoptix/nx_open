// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items

import nx.vms.client.core
import nx.vms.client.core.timeline as Timeline
import nx.vms.client.mobile.timeline as Timeline

import "./Timeline" as Timeline

Rectangle
{
    id: timeline

    property real positionMs: NxGlobals.syncNowMs()
    property bool positionAtLive: true //< Position marker is at live.
    readonly property bool positioning: dragHandler.draggingTimeMarker && dragHandler.active

    readonly property alias startTimeMs: timeScale.startTimeMs
    readonly property alias durationMs: timeScale.durationMs
    readonly property real endTimeMs: startTimeMs + durationMs
    property bool windowAtLive: true //< Window end is at live.

    property alias timeZone: timeScale.timeZone

    property ChunkProvider chunkProvider
    property alias objectsType: objects.objectsType

    property real zoomMultiplier: 1.5

    property real minimumDurationMs: 500

    readonly property real defaultDurationMs: 60 * 60 * 1000

    readonly property real bottomBoundMs:
        objects.bottomBoundMs ?? (endTimeMs - defaultDurationMs)

    property real timeScaleWidth: 70

    property bool interactive: true

    signal objectTileTapped(Timeline.MultiObjectData data)

    color: ColorTheme.colors.mobileTimeline.background

    // Whether the current window contains the specified time.
    function contains(timeMs)
    {
        return timeMs >= startTimeMs && (windowAtLive || timeMs < (startTimeMs + durationMs))
    }

    // Perform animated scroll to align the center of the window with the specified time.
    function scrollTo(timeMs)
    {
        const nowMs = NxGlobals.syncNowMs()
        const scrollToLive = timeMs == nowMs

        content.cancelAnimations()

        timeMs = Math.min(timeMs,
            nowMs + animatedScroll.duration - timeScale.durationMs / 2.0)

        animatedScroll.toLive = scrollToLive
        animatedScroll.from = startTimeMs
        animatedScroll.to = timeMs - durationMs / 2.0
        timeline.windowAtLive = false

        animatedScroll.start()
    }

    // Change current position and, if it's outside of the window, perform animated scroll to align
    // the center of the window with the new position.
    function setPosition(timeMs)
    {
        positionAtLive = false
        positionMs = timeMs

        if (!contains(positionMs))
            scrollTo(positionMs)
    }

    // Perform animated scroll and zoom into the specified window.
    function setWindow(newStartTimeMs, newDurationMs)
    {
        content.cancelAnimations()

        animatedScroll.from = startTimeMs
        animatedScroll.to = newStartTimeMs
        animatedZoom.from = durationMs
        animatedZoom.to = newDurationMs
        timeline.windowAtLive = false

        animatedScroll.start()
        animatedZoom.start()
    }

    // If the current position is outside of the current window, perform instant scroll to align
    // the closest edge of the window with the current position.
    function followPosition()
    {
        if (positionAtLive)
        {
            windowAtLive = true
        }
        else if (positionMs >= endTimeMs)
        {
            timeScale.startTimeMs = positionMs - durationMs + 1
        }
        else if (positionMs < startTimeMs)
        {
            windowAtLive = false
            timeScale.startTimeMs = positionMs
        }
    }

    // Perform instant zoom by the specified factor.
    // The zoom is performed either around the current position if it is within the window,
    // or around the center of the window otherwise.
    function zoom(factor)
    {
        if (!positionAtLive && contains(positionMs))
            timeScale.zoom(factor, timeScale.timeToPosition(positionMs))
        else
            timeScale.zoom(factor)
    }

    // Header bar with the current view date range.
    Rectangle
    {
        id: header

        color: ColorTheme.colors.mobileTimeline.header.background

        width: timeline.width
        height: 23

        Text
        {
            id: currentDateLabel

            text: timeScale.labelFormatter.formatHeader(
                timeline.startTimeMs, timeline.endTimeMs, timeline.timeZone)

            color: ColorTheme.colors.mobileTimeline.header.text

            anchors.left: header.left
            anchors.leftMargin: 12
            anchors.verticalCenter: header.verticalCenter

            font.pixelSize: 12
            font.weight: 400
        }

        Rectangle
        {
            color: ColorTheme.colors.mobileTimeline.header.underline
            width: header.width
            height: 1
            anchors.bottom: header.bottom
        }

        Rectangle
        {
            color: ColorTheme.colors.mobileTimeline.header.shadow
            width: header.width
            height: 1
            anchors.top: header.bottom
        }
    }

    Item
    {
        id: content

        anchors.topMargin: header.height
        anchors.fill: timeline

        function cancelAnimations()
        {
            kineticScroll.stop()
            animatedScroll.stop()
            animatedScroll.toLive = false
            animatedZoom.stop()
        }

        // Scale with ticks and labels.
        VerticalTimeScale
        {
            id: timeScale

            width: recordingChunks.width + timeline.timeScaleWidth
            clip: true

            startTimeMs: NxGlobals.syncNowMs() - durationMs
            durationMs: timeline.defaultDurationMs

            anchors.right: content.right
            height: content.height

            font.pixelSize: 12
            font.weight: 400

            textColor: ColorTheme.colors.mobileTimeline.scale.labels
            majorTickColor: ColorTheme.colors.mobileTimeline.scale.ticks

            function setPosition(y)
            {
                timeline.setPosition(positionToTime(Math.max(0, Math.min(y, height - 1))))
            }

            function setStartTimeMs(value)
            {
                const nowMs = NxGlobals.syncNowMs()
                timeline.windowAtLive = value + timeScale.durationMs >= nowMs

                timeScale.startTimeMs = Math.max(timeline.bottomBoundMs,
                    timeline.windowAtLive
                        ? (nowMs - timeScale.durationMs)
                        : value)
            }

            // Zoom around specified pivot y-coordinate.
            // If pivot is undefined, either the live position is chosen if `windowAtLive` is true,
            // or center of the window otherwise.
            function zoom(factor, pivot)
            {
                if (factor <= 0.001)
                    return

                if (pivot === undefined)
                {
                    if (timeline.windowAtLive)
                    {
                        // Zoom at live.
                        const nowMs = NxGlobals.syncNowMs()
                        const maximumDurationMs = nowMs - timeline.bottomBoundMs

                        timeScale.durationMs = Math.max(timeline.minimumDurationMs,
                            Math.min(maximumDurationMs, timeScale.durationMs * factor))

                        timeScale.startTimeMs = nowMs - timeScale.durationMs
                        return
                    }
                    else
                    {
                        // Choose center of the window.
                        pivot = height / 2.0
                    }
                }

                const oldPivotMs = positionToTime(pivot)
                timeline.windowAtLive = false

                const maximumDurationMs = NxGlobals.syncNowMs() - timeline.bottomBoundMs

                timeScale.durationMs = Math.max(timeline.minimumDurationMs,
                    Math.min(maximumDurationMs, timeScale.durationMs * factor))

                const newPivotMs = positionToTime(pivot)

                setStartTimeMs(Math.max(timeline.bottomBoundMs,
                    timeScale.startTimeMs + (oldPivotMs - newPivotMs)))
            }

            ProximityScrollHelper
            {
                id: proximityScrollHelper

                clipping: false
                baseVelocity: 10
                sensitiveAreaLength: inset + outset

                readonly property real inset: timeMarkerRect.height / 2.0
                readonly property real outset: 64

                property real lastTimeMs: 0

                function scrollByTimeMarker(currentTimeMs)
                {
                    if (!dragHandler.draggingTimeMarker)
                        return

                    const geometry = Qt.rect(
                        0, -outset, timeScale.width, timeScale.height + outset * 2)

                    const velocity = -proximityScrollHelper.velocity(
                        geometry, dragHandler.centroid.position)

                    if (!velocity)
                        return

                    const deltaMs = velocity * (currentTimeMs - lastTimeMs)
                        * timeScale.millisecondsPerPixel

                    const maxStartTimeMs = currentTimeMs - timeScale.durationMs

                    timeScale.startTimeMs = Math.max(timeline.bottomBoundMs,
                        Math.min(timeScale.startTimeMs + deltaMs, maxStartTimeMs))

                    timeline.windowAtLive = timeScale.startTimeMs === maxStartTimeMs

                    lastTimeMs = currentTimeMs
                    dragHandler.updatePosition(dragHandler.centroid.position.y)
                }
            }
        }

        component AnimatedStripePattern: StripePattern
        {
            id: animatedStripePattern

            property bool active: true
            visible: active

            stripeWidth: 2
            gapWidth: 3

            verticalOffset: y + t * period

            readonly property real period:
            {
                const angle = angleDegrees * (Math.PI / 180)
                return (stripeWidth + gapWidth) / Math.cos(angle)
            }

            property real t: 0

            NumberAnimation on t
            {
                from: 0.0
                to: 1.0
                duration: 500
                loops: Animation.Infinite
                running: animatedStripePattern.active
            }
        }

        // Timeline Objects.
        Timeline.ObjectsList
        {
            id: objects

            anchors.left: content.left
            anchors.leftMargin: 12
            anchors.right: timeScale.left
            height: content.height

            chunkProvider: timeline.chunkProvider
            minimumStackDurationMs: timeline.minimumDurationMs / (1.0 - zoomInMarginFraction * 2.0)

            startTimeMs: timeline.startTimeMs
            durationMs: timeline.durationMs
            windowAtLive: timeline.windowAtLive

            interactive: timeline.interactive

            property real zoomInMarginFraction: 0.1

            onTapped: (modelData) =>
            {
                if (modelData.count > 1 && modelData.durationMs >= minimumStackDurationMs)
                {
                    // Stack tapped. Zoom into.
                    const marginMs = Math.round(modelData.durationMs * zoomInMarginFraction)

                    timeline.setWindow(
                        modelData.positionMs - marginMs, modelData.durationMs + marginMs * 2)
                }
                else
                {
                    // Object tapped. Set playback position.
                    timeline.setPosition(modelData.positionMs)
                    timeline.objectTileTapped(modelData)
                }
            }

            preloaderDelegate: Component
            {
                Item
                {
                    id: preloader

                    AnimatedStripePattern
                    {
                        id: stipple

                        // Reparent this item to a child of `eventChunks`.
                        parent: chunkPreloaders
                        width: chunkPreloaders.width

                        y: preloader.parent.parent.y
                        height: preloader.parent.parent.height
                        visible: preloader.visible

                        color: eventChunks.chunkColor
                        backgroundColor: recordingChunks.backgroundColor
                    }
                }
            }
        }

        // Archive chunks.
        Timeline.ChunkBar
        {
            id: recordingChunks

            objectName: "recordingChunks"

            width: 8
            height: content.height
            anchors.left: timeScale.left
            mirrored: timeScale.direction === VerticalTimeScale.Upward

            chunks: recordingChunkProvider.chunks
            startTimeMs: timeline.startTimeMs
            durationMs: timeline.durationMs

            backgroundColor: ColorTheme.colors.mobileTimeline.chunks.background
            chunkColor: ColorTheme.colors.mobileTimeline.chunks.recording
        }

        TypedChunkProvider
        {
            id: recordingChunkProvider

            sourceProvider: timeline.chunkProvider
            contentType: ChunkProvider.RecordingContent
        }

        // Motion or Objects chunks.
        Timeline.ChunkBar
        {
            id: eventChunks

            objectName: "eventChunks"

            anchors.fill: recordingChunks
            mirrored: timeScale.direction === VerticalTimeScale.Upward

            chunks:
            {
                switch (timeline.objectsType)
                {
                    case Timeline.ObjectsLoader.ObjectsType.motion:
                        return motionChunkProvider.chunks

                    case Timeline.ObjectsLoader.ObjectsType.analytics:
                        return objectsChunkProvider.chunks

                    case Timeline.ObjectsLoader.ObjectsType.bookmarks:
                        return objects.objectChunks
                }

                console.assert(false, `Unknown objectsType (${timeline.objectsType})`)
                return []
            }

            startTimeMs: timeline.startTimeMs
            durationMs: timeline.durationMs

            backgroundColor: "transparent"

            chunkColor:
            {
                switch (timeline.objectsType)
                {
                    case Timeline.ObjectsLoader.ObjectsType.motion:
                        return ColorTheme.colors.mobileTimeline.chunks.motion

                    case Timeline.ObjectsLoader.ObjectsType.analytics:
                        return ColorTheme.colors.mobileTimeline.chunks.analytics

                    default:
                        return ColorTheme.colors.mobileTimeline.chunks.bookmarks
                }
            }

            Item
            {
                id: chunkPreloaders

                anchors.fill: eventChunks
                clip: true

                AnimatedStripePattern
                {
                    id: initialLoadingIndicator

                    active: color.a > 0.0
                    anchors.fill: chunkPreloaders

                    backgroundColor: recordingChunks.backgroundColor

                    color:
                    {
                        if (timeline.chunkProvider.loading)
                            return ColorTheme.colors.mobileTimeline.chunks.lastSecond

                        if (timeline.objectsType === Timeline.ObjectsLoader.ObjectsType.motion
                            && timeline.chunkProvider.loadingMotion)
                        {
                            return ColorTheme.colors.mobileTimeline.chunks.motion
                        }

                        if (timeline.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
                            && timeline.chunkProvider.loadingAnalytics)
                        {
                            return ColorTheme.colors.mobileTimeline.chunks.analytics
                        }

                        return "transparent"
                    }
                }

                AnimatedStripePattern
                {
                    id: lastSecondsIndicator

                    readonly property real kLastSecondsDurationMs: 3000

                    color: ColorTheme.colors.mobileTimeline.chunks.lastSecond
                    width: recordingChunks.width

                    height: kLastSecondsDurationMs / timeScale.millisecondsPerPixel
                    active: !initialLoadingIndicator.active

                    // Update y each animation step when animated parameter t changes.
                    y: t, timeScale.timeToPosition(NxGlobals.syncNowMs())
                }

                // This container is further populated by reparenting preloader delegates from
                // `objects` Timeline.ObjectsList.
            }
        }

        TypedChunkProvider
        {
            id: motionChunkProvider

            sourceProvider: timeline.chunkProvider
            contentType: ChunkProvider.MotionContent
            enabled: timeline.objectsType === Timeline.ObjectsLoader.ObjectsType.motion
        }

        TypedChunkProvider
        {
            id: objectsChunkProvider

            sourceProvider: timeline.chunkProvider
            contentType: ChunkProvider.AnalyticsContent
            enabled: timeline.objectsType === Timeline.ObjectsLoader.ObjectsType.analytics
        }

        // Current time marker ("current" as currently played).
        Item
        {
            id: timeMarker

            readonly property bool isAhead: timeline.positionAtLive
                || (timeline.positionMs > timeScale.startTimeMs + timeScale.durationMs)

            readonly property bool isBehind:
                timeline.positionMs < timeScale.startTimeMs

            readonly property bool isOutside: isAhead || isBehind

            readonly property real markerWidth:
            {
                return isOutside && !timeline.positionAtLive
                    ? timeMarkerText.width
                    : timeScale.width
            }

            readonly property real draggingShift: timeScale.width

            width: content.width
            height: 1

            y:
            {
                if (isAhead)
                    return timeScale.y - Math.ceil(header.height / 2.0)

                if (isBehind)
                    return timeScale.y - Math.ceil(header.height / 2.0) + timeScale.height

                return timeScale.y + timeScale.timeToPosition(timeline.positionMs)
            }

            Rectangle
            {
                id: timeMarkerLine

                color: ColorTheme.colors.mobileTimeline.timeMarker.background
                visible: !timeMarker.isOutside

                width: timeMarker.width
                height: 1
            }

            Rectangle
            {
                id: timeMarkerRect

                property real shift: dragHandler.draggingTimeMarker ? timeMarker.draggingShift : 0
                Behavior on shift { NumberAnimation { duration: 100 } }

                anchors.right: timeMarker.right
                anchors.rightMargin: shift
                anchors.verticalCenter: timeMarker.verticalCenter

                width: timeMarker.markerWidth - recordingChunks.width
                height: header.height
                clip: true
                color: ColorTheme.colors.mobileTimeline.timeMarker.background

                readonly property bool mirrored: timeline.LayoutMirroring.enabled
                topLeftRadius: mirrored ? 0 : 6
                topRightRadius: mirrored ? 6 : 0
                bottomLeftRadius: (mirrored || timeMarker.isOutside) ? 0 : 6
                bottomRightRadius: (mirrored && !timeMarker.isOutside) ? 6 : 0

                // Scrolls to the time marker when tapped.
                TapHandler
                {
                    id: timeMarkerTapHandler

                    acceptedButtons: Qt.LeftButton

                    enabled: timeline.interactive && timeMarker.isOutside

                    onTapped:
                    {
                        timeline.scrollTo(timeline.positionAtLive
                            ? NxGlobals.syncNowMs()
                            : timeline.positionMs)
                    }
                }
            }

            Text
            {
                id: timeMarkerText

                leftPadding: 16
                rightPadding: timeMarker.isOutside ? 12 : timeScale.labelOffset

                anchors.verticalCenter: timeMarker.verticalCenter
                anchors.right: timeMarkerRect.right

                text:
                {
                    if (timeline.positionAtLive)
                        return qsTr("Live")

                    if (timeMarker.isOutside)
                    {
                        return timeScale.labelFormatter.formatOutsideMarker(timeline.positionMs,
                            timeScale.timeZone, timeScale.zoomLevel)
                    }

                    return timeScale.labelFormatter.formatMarker(timeline.positionMs,
                        timeScale.timeZone, timeScale.millisecondsPerPixel)
                }

                color: ColorTheme.colors.mobileTimeline.timeMarker.text

                font.pixelSize: 12
                font.weight: 400
            }

            ColoredImage
            {
                id: liveIcon

                visible: timeline.positionAtLive

                anchors.right: timeMarkerText.left
                anchors.rightMargin: 2 - timeMarkerText.leftPadding
                anchors.verticalCenter: timeMarker.verticalCenter

                sourcePath: "image://skin/16x16/Solid/play_arrow.svg"
                sourceSize: Qt.size(16, 16)
                primaryColor: timeMarkerText.color
            }
        }

        // Positions the time marker when tapped.
        TapHandler
        {
            id: tapHandler

            acceptedButtons: Qt.LeftButton
            enabled: timeline.interactive

            onTapped: (point) =>
            {
                if (!timeMarkerRect.contains(
                    timeMarkerRect.mapFromItem(content, point.pressPosition)))
                {
                    content.cancelAnimations()
                    timeScale.setPosition(point.position.y)
                }
            }
        }

        // Drags the time marker and/or scrolls the timeline.
        DragHandler
        {
            id: dragHandler

            acceptedButtons: Qt.LeftButton
            enabled: timeline.interactive && !pinchHandler.active
            maximumPointCount: 1
            dragThreshold: 4
            target: null

            property bool draggingTimeMarker: false
            property bool draggingTimeline: false

            property real startTimeMs: 0

            function updatePosition(y)
            {
                if (kineticScroll.state === KineticAnimation.Stopped && !active)
                    return

                if (draggingTimeMarker)
                {
                    timeScale.setPosition(
                        Math.max(0, Math.min(timeScale.height - 1, y)))
                }
                else if (draggingTimeline)
                {
                    timeScale.setStartTimeMs(startTimeMs + (timeScale.millisecondsPerPixel
                        * (y - kineticScroll.startPosition.y)))
                }
            }

            onActiveChanged:
            {
                if (active)
                    startTimeMs = timeScale.startTimeMs

                if (draggingTimeline)
                {
                    if (active)
                        kineticScroll.startMeasurement(centroid.position)
                    else
                        kineticScroll.finishMeasurement(centroid.position)
                }
            }

            onCentroidChanged:
            {
                if (active)
                    kineticScroll.continueMeasurement(centroid.position)
            }

            onGrabChanged: (transition, eventPoint) =>
            {
                switch (transition)
                {
                    case PointerDevice.GrabPassive:
                    {
                        if (timeMarkerRect.contains(
                            timeMarkerRect.mapFromItem(content, centroid.pressPosition)))
                        {
                            proximityScrollHelper.lastTimeMs = NxGlobals.syncNowMs()
                            draggingTimeMarker = !timeMarker.isOutside
                        }
                        else
                        {
                            draggingTimeline = true
                        }

                        content.cancelAnimations()
                        break
                    }

                    case PointerDevice.UngrabPassive:
                    case PointerDevice.CancelGrabPassive:
                    {
                        draggingTimeMarker = false
                        break
                    }
                }
            }
        }

        KineticAnimation
        {
            id: kineticScroll

            onPositionChanged: (position) =>
                dragHandler.updatePosition(position.y)

            onStopped:
                dragHandler.draggingTimeline = false
        }

        // Zooms the timeline.
        PinchHandler
        {
            id: pinchHandler

            target: null
            enabled: timeline.interactive && !dragHandler.draggingTimeMarker
            dragThreshold: 1

            onScaleChanged: (delta) =>
                timeScale.zoom(1.0 / delta, centroid.position.y)

            onTranslationChanged: (delta) =>
            {
                timeScale.setStartTimeMs(timeScale.startTimeMs
                    + delta.y * timeScale.millisecondsPerPixel)
            }

            onActiveChanged:
            {
                if (active)
                    content.cancelAnimations()
            }
        }

        // Zooms the timeline.
        WheelHandler
        {
            onWheel: (wheel) =>
                timeScale.zoom(Math.pow(2.0, -wheel.angleDelta.y / 1200.0), wheel.y)
        }

        NumberAnimation
        {
            id: animatedScroll

            duration: 250
            easing.type: Easing.OutCubic

            target: timeScale
            property: "startTimeMs"

            property bool toLive: false

            onFinished:
            {
                if (toLive)
                    timeline.windowAtLive = true
            }
        }

        NumberAnimation
        {
            id: animatedZoom

            duration: 250
            easing.type: Easing.OutCubic

            target: timeScale
            property: "durationMs"
        }
    }

    Connections
    {
        id: d

        target: timeline.Window.window

        function onAfterAnimating()
        {
            const nowMs = NxGlobals.syncNowMs()

            if (timeline.positionAtLive)
                timeline.positionMs = nowMs
            else if (dragHandler.draggingTimeMarker)
                proximityScrollHelper.scrollByTimeMarker(nowMs)

            if (timeline.windowAtLive)
            {
                timeScale.startTimeMs = nowMs - timeScale.durationMs

                if (dragHandler.draggingTimeMarker)
                    dragHandler.updatePosition(dragHandler.centroid.position.y)
            }
        }
    }
}
