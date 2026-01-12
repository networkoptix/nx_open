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

    // Index of the cell which should be kept in the center of the screen during columns count
    // change animation.
    property int centeringIndex: -1

    function findIndexClosestToViewportCenter()
    {
        const viewportCenterY = flickable.contentY + flickable.height / 2

        let closestCellIndex = -1
        let minimumDistance = Number.MAX_VALUE

        for (let i = 0; i < repeater.count; ++i)
        {
            const loader = repeater.itemAt(i)
            const cellCenterY = loader.y + loader.height / 2
            const distance = Math.abs(cellCenterY - viewportCenterY)

            if (distance < minimumDistance)
            {
                minimumDistance = distance
                closestCellIndex = i
            }
        }

        return closestCellIndex
    }

    function contentYToCenterIndex(index)
    {
        if (index === -1)
            return flickable.contentY

        const loader = repeater.itemAt(index)
        if (!CoreUtils.assert(loader, "Invalid item index: " + index))
            return flickable.contentY

        return (loader.y + loader.height / 2) - flickable.height / 2
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
                const minModifier = 1
                const kMaxModifier = sizesCalculator.defaultColumnsCount

                // Limit logicalRotation to prevent unbounded accumulation
                const maxLogicalRotation = kMaxModifier * kDefaultRotationStep
                const minLogicalRotation = minModifier * kDefaultRotationStep

                logicalRotation = MathUtils.bound(
                    minLogicalRotation,
                    logicalRotation + event.angleDelta.y,
                    maxLogicalRotation)

                const newColumnsCountUserModifier =
                    Math.floor(logicalRotation / kDefaultRotationStep)

                if (newColumnsCountUserModifier === sizesCalculator.getUserDefinedColumnsCount())
                    return

                const anchorIndex = control.findIndexClosestToViewportCenter()
                if (anchorIndex >= 0)
                {
                    control.centeringIndex = anchorIndex
                    centeringTimer.restart()
                }

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

        // The cell closest to the viewport center at pinch start.
        property int anchorIndex: -1

        property bool hasGrab: false

        onActiveChanged:
        {
            if (active)
            {
                flickable.cancelFlick()
                baseColumnsCount = sizesCalculator.getUserDefinedColumnsCount()
                baseScale = activeScale
                anchorIndex = control.findIndexClosestToViewportCenter()
                return
            }

            // Keep centering for the last layout animation.
            if (control.centeringIndex >= 0)
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
                1,
                targetColumnsCount,
                sizesCalculator.defaultColumnsCount)

            if (targetColumnsCount === sizesCalculator.getUserDefinedColumnsCount())
                return

            if (anchorIndex < 0)
                anchorIndex = control.findIndexClosestToViewportCenter()

            control.centeringIndex = anchorIndex

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
    }

    Timer
    {
        id: centeringTimer

        interval: control.kColumnsAnimationDuration
        repeat: false

        onTriggered:
        {
            control.animationsEnabled = false

            if (pinchHandler.active || control.centeringIndex < 0)
                return

            const oldCenteringIndex = control.centeringIndex
            control.centeringIndex = -1
            flickable.contentY = control.contentYToCenterIndex(oldCenteringIndex)

            if (!pinchHandler.hasGrab)
                flickable.interactive = true
        }
    }

    Binding
    {
        target: flickable
        property: "contentY"
        restoreMode: Binding.RestoreNone
        when: control.centeringIndex >= 0 && (pinchHandler.active || centeringTimer.running)
        value: control.contentYToCenterIndex(control.centeringIndex)
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
}
