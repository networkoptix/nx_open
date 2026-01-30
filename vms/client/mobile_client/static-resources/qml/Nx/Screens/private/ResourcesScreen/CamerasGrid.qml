// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Mobile

import "CamerasGrid_private"

Item
{
    id: control

    property alias layout: camerasModel.layout
    property alias count: repeater.count
    property bool keepStatuses: false
    property bool active: false
    property alias filterIds: camerasModel.filterIds
    property int spacing: 2

    property int leftMargin: 0
    property int rightMargin: 0
    property int topMargin: 0
    property int bottomMargin: 0

    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    function getMediaPlayer(index)
    {
        const loader = repeater.itemAt(index)
        if (!CoreUtils.assert(loader, "Invalid item index: " + index))
            return null

        return loader && loader.item ? loader.item.mediaPlayer : null
    }

    Flickable
    {
        id: flickable

        anchors.fill: parent
        anchors.leftMargin: parent.leftMargin
        anchors.rightMargin: parent.rightMargin
        anchors.topMargin: parent.topMargin
        anchors.bottomMargin: parent.bottomMargin

        contentWidth: width
        contentHeight: layoutItem.implicitHeight

        clip: true
        boundsBehavior: Flickable.StopAtBounds

        property real previousContentY: -1

        onWidthChanged: doDelayedGeometryUpdate()
        onHeightChanged: doDelayedGeometryUpdate()
        onContentYChanged:
        {
            if (geometryChangeAccumulatorTimer.running)
                return

            updatePlayersStates()
            d.saveLayoutPosition()
        }
        onContentHeightChanged: updatePlayersStates()

        Component.onCompleted:
        {
            sizesCalculator.availableWidth = width
            sizesCalculator.availableHeight = height
        }

        // Postponing geometry updates is required because the width and height change several
        // times during a single display rotation - we want to handle only the last state.
        function doDelayedGeometryUpdate()
        {
            if (!geometryChangeAccumulatorTimer.running)
                previousContentY = contentY

            resizeOverlay.visible = true

            geometryChangeAccumulatorTimer.restart()
        }

        function updateSizesCalculatorAvailableGeometry()
        {
            if (sizesCalculator.availableWidth === width
                && sizesCalculator.availableHeight === height)
            {
                return
            }

            if (previousContentY < 0)
            {
                sizesCalculator.availableWidth = width
                sizesCalculator.availableHeight = height
            }
            else
            {
                const centerIndex = cellsSearcher.findCellClosestToViewportCenter(
                    previousContentY, sizesCalculator.availableWidth, sizesCalculator.availableHeight)

                previousContentY = -1
                sizesCalculator.availableWidth = width
                sizesCalculator.availableHeight = height

                if (centerIndex === -1)
                    return //< Just in case.

                contentY = contentYToCenterIndexStatic(centerIndex)
            }

            d.saveTargetColumnsCount()

            resizeOverlay.visible = false
        }

        // Required for contentY update during screen resize. Must use statically calculated
        // geometry of items.
        function contentYToCenterIndexStatic(centerIndex)
        {
            if (sizesCalculator.cellsCount === 0)
                return 0

            const cellGeometry = geometryCalculator.calculateCellGeometry(centerIndex)
            const lastCellGeometry =
                geometryCalculator.calculateCellGeometry(sizesCalculator.cellsCount - 1)
            const currentContentHeight = lastCellGeometry.y + lastCellGeometry.height

            // Calculate target contentY to preserve the cell's relative position in viewport.
            const targetContentY = cellGeometry.y
                - (sizesCalculator.availableHeight - cellGeometry.height)
                    * d.kCenterAnchorRelativePosition

            if (targetContentY < 0)
                return 0

            if (targetContentY + sizesCalculator.availableHeight > currentContentHeight)
            {
                // Handle the case when the content height is less than the viewport height.
                return Math.max(0, currentContentHeight - sizesCalculator.availableHeight)
            }

            return d.ensureCellIsFullyVisible(
                targetContentY, cellGeometry, sizesCalculator.availableHeight)
        }

        function updatePlayersStates()
        {
            for (let index = 0; index < repeater.count; ++index)
            {
                let cameraLoaderItem = repeater.itemAt(index)
                if (cameraLoaderItem)
                    cameraLoaderItem.updatePlayerState()
            }
        }

        Timer
        {
            id: geometryChangeAccumulatorTimer

            interval: 40 //< A bit more than 2 frames at 60fps. The value is chosen approximately.
            repeat: false

            onTriggered:
            {
                flickable.updateSizesCalculatorAvailableGeometry()
                flickable.updatePlayersStates()
                d.saveLayoutPosition()
            }
        }

        Item
        {
            id: layoutItem

            width: parent.width
            implicitHeight: sizesCalculator.totalLayoutHeight

            Repeater
            {
                id: repeater

                model: QnCameraListModel
                {
                    id: camerasModel

                    function updateControlState()
                    {
                        d.animationsEnabled = false
                        Qt.callLater(
                            function()
                            {
                                d.loadTargetColumnsCount()
                                d.loadLayoutPosition()
                            })
                    }

                    onSystemContextsSetChanged: updateControlState()
                    onLayoutChanged: updateControlState()
                }

                delegate: Loader
                {
                    id: loader

                    required property int index
                    required property string resourceName
                    required property int resourceStatus
                    required property string thumbnail
                    required property var resource

                    readonly property var geometry: geometryCalculator.calculateCellGeometry(index)
                    x: geometry.x
                    y: geometry.y
                    width: geometry.width
                    height: geometry.height

                    Behavior on x
                    {
                        enabled: d.animationsEnabled
                        onEnabledChanged: if (!enabled) xAnimation.complete()

                        NumberAnimation
                        {
                            id: xAnimation
                            duration: d.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on y
                    {
                        enabled: d.animationsEnabled
                        onEnabledChanged: if (!enabled) yAnimation.complete()

                        NumberAnimation
                        {
                            id: yAnimation
                            duration: d.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on width
                    {
                        enabled: d.animationsEnabled
                        onEnabledChanged: if (!enabled) widthAnimation.complete()

                        NumberAnimation
                        {
                            id: widthAnimation
                            duration: d.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on height
                    {
                        enabled: d.animationsEnabled
                        onEnabledChanged: if (!enabled) heightAnimation.complete()

                        NumberAnimation
                        {
                            id: heightAnimation
                            duration: d.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    active:
                    {
                        const margin = sizesCalculator.columnsCount <= 2
                            ? flickable.height
                            : flickable.height / 2
                        const viewTop = flickable.contentY - margin
                        const viewBottom = flickable.contentY + flickable.height + margin
                        const center = y + height / 2

                        let newState = center >= viewTop && center <= viewBottom
                        if (d.animationsEnabled)
                            newState = newState || active

                        return newState
                    }

                    function updatePlayerState()
                    {
                        if (!active)
                            return

                        if (sizesCalculator.columnsCount <= 2)
                        {
                            item.setPlayerActive(true)
                            return
                        }

                        const visibleAreaTop = flickable.contentY
                        const visibleAreaBottom = flickable.contentY + flickable.height

                        const itemIsVisible =
                            loader.y + loader.height > visibleAreaTop
                                && loader.y < visibleAreaBottom

                        item.setPlayerActive(itemIsVisible)
                    }

                    sourceComponent: Item
                    {
                        function setPlayerActive(active)
                        {
                            cameraItem.setPlayerActive(active)
                        }

                        CameraItem
                        {
                            id: cameraItem

                            anchors.fill: parent

                            aspectRatio: sizesCalculator.kAspectRatio

                            keepStatus: control.keepStatuses
                            active: control.active

                            resourceName: loader.resourceName
                            status: loader.resourceStatus
                            thumbnail: loader.thumbnail
                            resource: loader.resource

                            property bool mediaPlayerActive: false

                            onClicked:
                            {
                                // CamerasGrid changes sizes on camera opening, so we must disable
                                // contentY centering to prevent excessive jump.
                                flickable.previousContentY = -1

                                control.openVideoScreen(resource, thumbnail, camerasModel)
                            }

                            onThumbnailRefreshRequested: camerasModel.refreshThumbnail(index)

                            Component.onCompleted: updatePlayerState()
                            onMediaPlayerChanged: updatePlayerState()

                            function setPlayerActive(active)
                            {
                                mediaPlayerActive = active
                                updatePlayerState()
                            }

                            function updatePlayerState()
                            {
                                if (!mediaPlayer)
                                    return

                                if (mediaPlayerActive && !mediaPlayer.playing)
                                    mediaPlayer.playLive()
                                else if (!mediaPlayerActive && mediaPlayer.playing)
                                    mediaPlayer.stop()
                            }
                        }
                    }
                }
            }
        }

        WheelHandler
        {
            id: wheelHandler

            property real logicalRotation: 0
            readonly property real kDefaultRotationStep: 120

            acceptedDevices: PointerDevice.Mouse

            onWheel: (event) =>
            {
                // Limit logicalRotation to prevent unbounded accumulation.
                const minLogicalRotation =
                    sizesCalculator.minColumnsCount * kDefaultRotationStep
                const maxLogicalRotation =
                    sizesCalculator.defaultColumnsCount * kDefaultRotationStep

                const newLogicalRotation = logicalRotation + event.angleDelta.y

                if ((newLogicalRotation < minLogicalRotation
                        || newLogicalRotation > maxLogicalRotation)
                    && sizesCalculator.widthFillingZoomAvailable)
                {
                    const newWidthFillingZoomEnabledValue = newLogicalRotation < minLogicalRotation

                    if (sizesCalculator.widthFillingZoomEnabled()
                        === newWidthFillingZoomEnabledValue)
                    {
                        return
                    }

                    updateAnchor()

                    d.animationsEnabled = true
                    sizesCalculator.setWidthFillingZoomEnabled(newWidthFillingZoomEnabledValue)
                }
                else
                {
                    logicalRotation = MathUtils.bound(
                        minLogicalRotation,
                        newLogicalRotation,
                        maxLogicalRotation)

                    const newColumnsCountUserModifier =
                        Math.floor(logicalRotation / kDefaultRotationStep)

                    if (newColumnsCountUserModifier
                        === sizesCalculator.getUserDefinedColumnsCount())
                    {
                        return
                    }

                    updateAnchor()

                    d.animationsEnabled = true
                    sizesCalculator.setUserDefinedColumnsCount(newColumnsCountUserModifier)
                }

                centeringTimer.restart()
                d.saveTargetColumnsCount()
            }

            function updateAnchor()
            {
                // If animation is already running, keep the current anchor.
                // Otherwise, find new anchor based on current viewport.
                if (d.anchorIndex >= 0)
                    return

                d.anchorIndex = cellsSearcher.findCellClosestToViewportCenter(
                    flickable.contentY, flickable.width, flickable.height)
                d.anchorRelativePosition = d.kCenterAnchorRelativePosition
            }

            function updateStartLogicalRotation()
            {
                const currentUserModifier = sizesCalculator.getUserDefinedColumnsCount()
                logicalRotation = currentUserModifier * kDefaultRotationStep
            }
        }
    }

    Rectangle
    {
        id: topShadow

        readonly property real kMaxOpacity: 1
        readonly property int kAnimationZoneHeight: sizesCalculator.kMinCellHeight

        readonly property real animationFactor:
            MathUtils.bound(0, flickable.contentY / kAnimationZoneHeight, 1)

        x: flickable.x
        y: flickable.y
        width: flickable.width
        height: 32

        visible: opacity > 0
        opacity: kMaxOpacity * animationFactor

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: "#E5000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }

    Rectangle
    {
        readonly property real animationFactor: MathUtils.bound(
            0,
            (flickable.contentHeight - flickable.height - flickable.contentY)
                / topShadow.kAnimationZoneHeight,
            1)

        x: flickable.x
        // +1 is added, because for an unknown reason, 1px gap appears otherwise,
        // for some window size.
        y: flickable.y + flickable.height - height + 1

        width: flickable.width
        height: topShadow.height

        visible: opacity > 0
        opacity: topShadow.kMaxOpacity * animationFactor

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#E5000000" }
        }
    }

    // Overlay shown during resize to hide intermediate states.
    Rectangle
    {
        id: resizeOverlay

        anchors.fill: parent
        color: "black"

        visible: false
    }

    PinchHandler
    {
        id: pinchHandler

        target: null
        acceptedDevices: PointerDevice.TouchScreen
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        minimumPointCount: 2
        maximumPointCount: 2

        // Gesture reference point.
        property int baseColumnsCount: 0
        property real baseScale: 1.0

        // Initial columns count at gesture start to limit total change per gesture.
        property int initialColumnsCount: 0

        property bool hasGrab: false

        onActiveChanged:
        {
            if (active)
            {
                flickable.cancelFlick()
                baseColumnsCount = sizesCalculator.columnsCount
                baseScale = activeScale

                // Remember initial state to limit total change during the gesture.
                initialColumnsCount = baseColumnsCount

                // If the animation is already running, keep the current anchor and ignore the new
                // pinch point position.
                // Otherwise, find a new anchor based on the current viewport.
                if (d.anchorIndex < 0)
                    updateAnchor()

                return
            }

            // Keep centering for the last layout animation.
            if (d.anchorIndex >= 0)
                centeringTimer.restart()

            d.saveTargetColumnsCount()
        }

        onActiveScaleChanged:
        {
            if (!active)
                return

            if (baseScale <= 0.0)
                baseScale = 1.0

            const relativeScale = activeScale / baseScale
            let targetColumnsCount = Math.round(baseColumnsCount / relativeScale)

            if (sizesCalculator.widthFillingZoomAvailable)
            {
                // Zoom beyond minColumnsCount into width filling zoom.

                if (targetColumnsCount === sizesCalculator.minColumnsCount)
                    return

                var newWidthFillingZoomEnabledValue =
                    targetColumnsCount < sizesCalculator.minColumnsCount

                if (sizesCalculator.widthFillingZoomEnabled() === newWidthFillingZoomEnabledValue)
                    return
            }
            else
            {
                // Normal zoom within allowed columns count range.

                targetColumnsCount = MathUtils.bound(
                    sizesCalculator.minColumnsCount,
                    targetColumnsCount,
                    sizesCalculator.defaultColumnsCount)

                // Limit total change to +-1 column per gesture relative to initial state.
                targetColumnsCount = MathUtils.bound(
                    initialColumnsCount - 1,
                    targetColumnsCount,
                    initialColumnsCount + 1)

                if (targetColumnsCount === sizesCalculator.columnsCount)
                    return
            }

            // Common zoom logic.

            if (d.anchorIndex < 0)
                updateAnchor()
            const targetContentY = d.contentYToCenterIndexDynamic(d.anchorIndex)
            d.animationsEnabled = true

            if (sizesCalculator.widthFillingZoomAvailable)
            {
                // Zoom beyond minColumnsCount into width filling zoom.

                sizesCalculator.setWidthFillingZoomEnabled(newWidthFillingZoomEnabledValue)
                baseColumnsCount = 1
            }
            else
            {
                // Normal zoom within allowed columns count range.

                sizesCalculator.setUserDefinedColumnsCount(targetColumnsCount)
                baseColumnsCount = targetColumnsCount
            }

            // Common zoom logic.

            // For an unknown  reason, contentY jumps to 0 after cells geometry change.
            // So, we restore it to the correct state.
            flickable.contentY = targetContentY
            baseScale = activeScale
        }

        onGrabChanged: (transition) =>
        {
            hasGrab = transition === PointerDevice.GrabExclusive
                || transition === PointerDevice.GrabPassive

            flickable.interactive = !hasGrab && !centeringTimer.running
        }

        function updateAnchor()
        {
            const scenePoint = centroid.scenePosition
            const flickablePoint = flickable.mapFromItem(null, scenePoint.x, scenePoint.y)
            const cellIndex = cellsSearcher.findClosestFullyVisibleCell(
                flickablePoint, flickable.contentY, flickable.height)
            // Relative position must be calculated before setting anchorIndex.
            d.anchorRelativePosition = calculateAnchorRelativePosition(cellIndex)
            d.anchorIndex = cellIndex
        }

        function calculateAnchorRelativePosition(index)
        {
            if (index < 0)
                return d.kCenterAnchorRelativePosition

            const cellItem = repeater.itemAt(index)
            if (!cellItem)
                return d.kCenterAnchorRelativePosition

            const cellTopRelativeToViewport = cellItem.y - flickable.contentY
            const availableSpace = flickable.height - cellItem.height
            if (availableSpace <= 0)
                return d.kCenterAnchorRelativePosition

            const relativePosition = cellTopRelativeToViewport / availableSpace
            return MathUtils.bound(0, relativePosition, 1)
        }
    }

    Timer
    {
        id: centeringTimer

        interval: d.kColumnsAnimationDuration
        repeat: false

        onTriggered:
        {
            d.animationsEnabled = false

            if (pinchHandler.active || d.anchorIndex < 0)
                return

            const oldAnchorIndex = d.anchorIndex
            d.anchorIndex = -1

            if (CoreUtils.assert(oldAnchorIndex < 0 || oldAnchorIndex < repeater.count,
                "Invalid item index: " + oldAnchorIndex))
            {
                flickable.contentY = d.contentYToCenterIndexDynamic(oldAnchorIndex)
            }

            if (!pinchHandler.hasGrab)
                flickable.interactive = true
        }
    }

    Binding
    {
        target: flickable
        property: "contentY"
        restoreMode: Binding.RestoreNone
        when: d.anchorIndex >= 0 && (pinchHandler.active || centeringTimer.running)
        value: d.contentYToCenterIndexDynamic(d.anchorIndex)
    }

    SizesCalculator
    {
        id: sizesCalculator

        cellsCount: repeater.count
        spacing: control.spacing
    }

    GeometryCalculator
    {
        id: geometryCalculator

        sizesCalculator: sizesCalculator
    }

    CellsSearcher
    {
        id: cellsSearcher

        sizesCalculator: sizesCalculator
        geometryCalculator: geometryCalculator
    }

    CamerasGridHelper
    {
        id: camerasGridHelper
    }

    NxObject
    {
        id: d

        readonly property int kColumnsAnimationDuration: 250
        readonly property real kCenterAnchorRelativePosition: 0.5

        // Is requred for disabling animation when column count change isn't in progress.
        // Otherwise, animation would be applied on every cell adding/removing.
        property bool animationsEnabled: false

        // Index of the cell which should be kept at the screen during columns count change
        // animation.
        property int anchorIndex: -1

        // Stores the relative position of the anchored cell within the viewport before pinch
        // gesture.
        property real anchorRelativePosition: 0.0

        function saveTargetColumnsCount()
        {
            const theOnlyColumnStretchMode =
                sizesCalculator.columnsCount === 1 && sizesCalculator.widthFillingZoomEnabled()

            camerasGridHelper.saveTargetColumnsCount(
                layout,
                theOnlyColumnStretchMode ? 0 : sizesCalculator.columnsCount)
        }

        function loadTargetColumnsCount()
        {
            const loadedTargetColumnsCount = camerasGridHelper.loadTargetColumnsCount(layout)

            if (loadedTargetColumnsCount === 0)
            {
                sizesCalculator.setUserDefinedColumnsCount(1)
                sizesCalculator.setWidthFillingZoomEnabled(true)
                return
            }

            sizesCalculator.setUserDefinedColumnsCount(loadedTargetColumnsCount)
            sizesCalculator.setWidthFillingZoomEnabled(false)

            wheelHandler.updateStartLogicalRotation()
        }

        function saveLayoutPosition()
        {
            camerasGridHelper.saveLayoutPosition(layout, flickable.contentY)
        }

        function loadLayoutPosition()
        {
            let position = camerasGridHelper.loadLayoutPosition(layout)
            if (position < 0 || flickable.height > flickable.contentHeight)
                position = 0
            else if (position > (flickable.contentHeight - flickable.height))
                position = flickable.contentHeight - flickable.height

            flickable.previousContentY = -1
            flickable.contentY = position
        }

        // Required for contentY update during wheel/pinch columns count change animation.
        // Must use actuall geometry of items, not calculated statically.
        // Must be defined locally to initiate binding updates when needed.
        function contentYToCenterIndexDynamic(centerIndex)
        {
            if (centerIndex < 0 || centerIndex >= repeater.count)
                return flickable.contentY

            const cellItem = repeater.itemAt(centerIndex)
            const targetContentY =
                cellItem.y - (flickable.height - cellItem.height) * anchorRelativePosition
            if (targetContentY < 0)
                return 0

            const lastItem = repeater.itemAt(repeater.count - 1)
            const currentContentHeight = lastItem.y + lastItem.height
            if (targetContentY + flickable.height > currentContentHeight)
                return Math.max(0, currentContentHeight - flickable.height)

            return ensureCellIsFullyVisible(targetContentY, cellItem, flickable.height)
        }

        // Ensure the cell is fully visible. If cell doesn't fit in viewport,
        // adjust minimally to show it fully.
        function ensureCellIsFullyVisible(targetContentY, cellGeometry, totalHeight)
        {
            const cellTop = cellGeometry.y
            const cellBottom = cellGeometry.y + cellGeometry.height
            const viewportTop = targetContentY
            const viewportBottom = targetContentY + totalHeight

            if (cellTop < viewportTop)
                return cellTop
            if (cellBottom > viewportBottom)
                return cellBottom - totalHeight

            return targetContentY
        }
    }
}
