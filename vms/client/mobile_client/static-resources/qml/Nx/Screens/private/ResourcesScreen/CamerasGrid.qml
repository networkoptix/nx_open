// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Window

import Nx.Core
import Nx.Mobile

import "CamerasGrid_private"

Item
{
    id: control

    readonly property int kColumnsAnimationDuration: 250
    readonly property real kCenterAnchorRelativePosition: 0.5

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

    property bool animationsEnabled: false

    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    // Index of the cell which should be kept at the screen during columns count change animation.
    property int anchorIndex: -1

    // Stores the relative position of the anchored cell within the viewport before pinch gesture.
    property real anchorRelativePosition: 0.0

    function contentYToCenterIndex(index)
    {
        if (index === -1)
            return 0 //< Strange case, most possibly the grid is empty.

        if (repeater.count === 0)
            return 0

        const cellItem = repeater.itemAt(index)
        if (!CoreUtils.assert(cellItem, "Invalid item index: " + index))
            return flickable.contentY

        // Calculate target contentY to preserve the cell's relative position in viewport.
        let targetContentY =
            cellItem.y - (flickable.height - cellItem.height) * anchorRelativePosition

        // Ensure the cell is fully visible with minimal adjustment.
        const cellTop = cellItem.y
        const cellBottom = cellItem.y + cellItem.height
        const viewportTop = targetContentY
        const viewportBottom = targetContentY + flickable.height

        if (targetContentY < 0)
            return 0

        const lastItem = repeater.itemAt(repeater.count - 1)
        const currentContentHeight = lastItem.y + lastItem.height
        // We cannot use flickable.contentHeight here, because it contains the final value,
        // after the layout animation is complete.
        if (targetContentY + flickable.height > currentContentHeight)
        {
            // Handle the case when the content height is less than the viewport height.
            return Math.max(0, currentContentHeight - flickable.height)
        }

        // If cell doesn't fit in viewport, adjust minimally to show it fully.
        if (cellTop < viewportTop)
            return cellTop
        if (cellBottom > viewportBottom)
            return cellBottom - flickable.height

        return targetContentY
    }

    function getMediaPlayer(index)
    {
        const loader = repeater.itemAt(index)
        if (!CoreUtils.assert(loader, "Invalid item index: " + index))
            return null

        return loader && loader.item ? loader.item.mediaPlayer : null
    }

    function saveTargetColumnsCount()
    {
        const columnCountToSave =
            sizesCalculator.columnsCount !== sizesCalculator.defaultColumnsCount
                ? sizesCalculator.columnsCount
                : sizesCalculator.kInvalidColumnsCount //< Remove saved value.

        camerasGridHelper.saveTargetColumnsCount(layout, columnCountToSave)
    }

    function loadTargetColumnsCount()
    {
        const loadedTargetColumnsCount = camerasGridHelper.loadTargetColumnsCount(layout)

        sizesCalculator.setUserDefinedColumnsCount(
            loadedTargetColumnsCount !== sizesCalculator.kInvalidColumnsCount
                ? loadedTargetColumnsCount
                : sizesCalculator.defaultColumnsCount)

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

        flickable.contentY = position
    }

    CamerasGridHelper
    {
        id: camerasGridHelper
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

        onWidthChanged: updatePlayersStates()
        onHeightChanged: updatePlayersStates()
        onContentYChanged:
        {
            updatePlayersStates()
            control.saveLayoutPosition()
        }
        onContentHeightChanged: updatePlayersStates()

        function updatePlayersStates()
        {
            for (let index = 0; index < repeater.count; ++index)
            {
                let cameraLoaderItem = repeater.itemAt(index)
                if (!cameraLoaderItem)
                    continue

                cameraLoaderItem.updatePlayerState()
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
                        control.animationsEnabled = false
                        Qt.callLater(
                            function()
                            {
                                control.loadTargetColumnsCount()
                                control.loadLayoutPosition()
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
                        enabled: control.animationsEnabled
                        onEnabledChanged: if (!enabled) xAnimation.complete()

                        NumberAnimation
                        {
                            id: xAnimation
                            duration: control.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on y
                    {
                        enabled: control.animationsEnabled
                        onEnabledChanged: if (!enabled) yAnimation.complete()

                        NumberAnimation
                        {
                            id: yAnimation
                            duration: control.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on width
                    {
                        enabled: control.animationsEnabled
                        onEnabledChanged: if (!enabled) widthAnimation.complete()

                        NumberAnimation
                        {
                            id: widthAnimation
                            duration: control.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on height
                    {
                        enabled: control.animationsEnabled
                        onEnabledChanged: if (!enabled) heightAnimation.complete()

                        NumberAnimation
                        {
                            id: heightAnimation
                            duration: control.kColumnsAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    active:
                    {
                        const margin = flickable.height / 2
                        const viewTop = flickable.contentY - margin
                        const viewBottom = flickable.contentY + flickable.height + margin
                        const center = y + height / 2
                        return center >= viewTop && center <= viewBottom
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

                            onClicked: control.openVideoScreen(
                                loader.resource, loader.thumbnail, camerasModel)

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

                logicalRotation = MathUtils.bound(
                    minLogicalRotation,
                    logicalRotation + event.angleDelta.y,
                    maxLogicalRotation)

                const newColumnsCountUserModifier =
                    Math.floor(logicalRotation / kDefaultRotationStep)

                if (newColumnsCountUserModifier === sizesCalculator.getUserDefinedColumnsCount())
                    return

                control.anchorIndex = cellsSearcher.findCellClosestToViewportCenter()
                control.anchorRelativePosition = kCenterAnchorRelativePosition
                centeringTimer.restart()

                control.animationsEnabled = true
                sizesCalculator.setUserDefinedColumnsCount(newColumnsCountUserModifier)
                control.saveTargetColumnsCount()
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

        property bool hasGrab: false

        onActiveChanged:
        {
            if (active)
            {
                flickable.cancelFlick()
                baseColumnsCount = sizesCalculator.getUserDefinedColumnsCount()
                baseScale = activeScale

                updateAnchor()

                return
            }

            // Keep centering for the last layout animation.
            if (control.anchorIndex >= 0)
                centeringTimer.restart()

            control.saveTargetColumnsCount()
        }

        onActiveScaleChanged:
        {
            if (!active)
                return

            if (baseScale <= 0.0)
                baseScale = 1.0

            const relativeScale = activeScale / baseScale

            let targetColumnsCount = Math.round(baseColumnsCount / relativeScale)
            targetColumnsCount = MathUtils.bound(
                sizesCalculator.minColumnsCount,
                targetColumnsCount,
                sizesCalculator.defaultColumnsCount)

            if (targetColumnsCount === sizesCalculator.getUserDefinedColumnsCount())
                return

            if (control.anchorIndex < 0)
                updateAnchor()

            control.animationsEnabled = true
            sizesCalculator.setUserDefinedColumnsCount(targetColumnsCount)

            baseColumnsCount = sizesCalculator.getUserDefinedColumnsCount()
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
            const cellIndex = cellsSearcher.findClosestFullyVisibleCell(centroid.scenePosition)
            // Relative position must be calculated before setting anchorIndex.
            control.anchorRelativePosition = calculateAnchorRelativePosition(cellIndex)
            control.anchorIndex = cellIndex
        }

        function calculateAnchorRelativePosition(index)
        {
            if (index < 0)
                return kCenterAnchorRelativePosition

            const cellItem = repeater.itemAt(index)
            if (!cellItem)
                return kCenterAnchorRelativePosition

            const cellTopRelativeToViewport = cellItem.y - flickable.contentY
            const availableSpace = flickable.height - cellItem.height
            if (availableSpace <= 0)
                return kCenterAnchorRelativePosition

            const relativePosition = cellTopRelativeToViewport / availableSpace
            return MathUtils.bound(0, relativePosition, 1)
        }
    }

    Timer
    {
        id: centeringTimer

        interval: control.kColumnsAnimationDuration
        repeat: false

        onTriggered:
        {
            control.animationsEnabled = false

            if (pinchHandler.active || control.anchorIndex < 0)
                return

            const oldAnchorIndex = control.anchorIndex
            control.anchorIndex = -1
            flickable.contentY = control.contentYToCenterIndex(oldAnchorIndex)

            if (!pinchHandler.hasGrab)
                flickable.interactive = true
        }
    }

    Binding
    {
        target: flickable
        property: "contentY"
        restoreMode: Binding.RestoreNone
        when: control.anchorIndex >= 0 && (pinchHandler.active || centeringTimer.running)
        value: control.contentYToCenterIndex(control.anchorIndex)
    }

    SizesCalculator
    {
        id: sizesCalculator

        cellsCount: repeater.count
        availableWidth: flickable.width
        availableHeight: flickable.height
        spacing: control.spacing
    }

    GeometryCalculator
    {
        id: geometryCalculator
    }

    CellsSearcher
    {
        id: cellsSearcher

        repeater: repeater
        flickable: flickable
        spacing: control.spacing
    }
}
