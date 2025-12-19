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
    property alias spacing: flow.spacing

    property int leftMargin: 0
    property int rightMargin: 0
    property int topMargin: 0
    property int bottomMargin: 0

    signal openVideoScreen(var resource, var thumbnailUrl, var camerasModel)

    function getMediaPlayer(cameraIndex)
    {
        const loader = repeater.itemAt(cameraIndex)
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

        contentWidth: width
        contentHeight: flow.implicitHeight

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

        Flow
        {
            id: flow

            width: parent.width

            spacing: 2

            Repeater
            {
                id: repeater

                model: QnCameraListModel
                {
                    id: camerasModel

                    function updateControlState()
                    {
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

                    readonly property int itemWidth: index < sizesCalculator.enlargedCellsCount
                        ? sizesCalculator.enlargedCellWidth
                        : sizesCalculator.normalCellWidth
                    readonly property int itemHeight:
                        sizesCalculator.calculateCellHeight(itemWidth)

                    active:
                    {
                        const margin = flickable.height
                        const viewTop = flickable.contentY - margin
                        const viewBottom = flickable.contentY + flickable.height + margin
                        const center = y + itemHeight / 2
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
                            loader.y + loader.itemHeight > visibleAreaTop
                                && loader.y < visibleAreaBottom

                        item.setPlayerActive(itemIsVisible)
                    }

                    sourceComponent: Item
                    {
                        width: cameraItem.x + cameraItem.width
                        height: cameraItem.height

                        function setPlayerActive(active)
                        {
                            cameraItem.setPlayerActive(active)
                        }

                        CameraItem
                        {
                            id: cameraItem

                            x: {
                                if (sizesCalculator.columnsCount === 1)
                                    return paddingsCalculator.normalCellsPadding

                                // Enlarged top rows.
                                if (rowIndexChecker.isEnlargedCell(index))
                                {
                                    if (rowIndexChecker.isFirstEnlargedRowBeginning(index))
                                        return paddingsCalculator.firstEnlargedRowPadding

                                    if (!paddingsCalculator.singleRowIsEnlarged
                                        && rowIndexChecker.isSecondEnlargedRowBeginning(index))
                                    {
                                        return paddingsCalculator.secondEnlargedRowPadding
                                    }

                                    return paddingsCalculator.kPaddingOfCellsInTheMiddleOfRow
                                }

                                // Repositioned tail cells.
                                if (rowIndexChecker.isRepositionedTailCell(index))
                                {
                                    if (rowIndexChecker.isFirstRepositionedTailRowBeginning(index))
                                        return paddingsCalculator.firstRepositionedRowPadding

                                    if (!paddingsCalculator.singleRowIsRepositioned
                                        && rowIndexChecker.isSecondRepositionedTailRowBeginning(index))
                                    {
                                        return paddingsCalculator.secondRepositionedRowPadding
                                    }

                                    return paddingsCalculator.kPaddingOfCellsInTheMiddleOfRow
                                }

                                // Normal cells.
                                const indexInNormalCells =
                                    index - sizesCalculator.enlargedCellsCount
                                const isAtTheBeginningOfRow =
                                    indexInNormalCells % sizesCalculator.columnsCount === 0

                                return isAtTheBeginningOfRow
                                    ? paddingsCalculator.normalCellsPadding
                                    : paddingsCalculator.kPaddingOfCellsInTheMiddleOfRow
                            }

                            width: itemWidth
                            height: itemHeight

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
            acceptedDevices: PointerDevice.Mouse
            property real logicalRotation: 0

            onWheel: (event) =>
            {
                if (transitionOverlay.layoutTransitionRunning)
                    return

                logicalRotation += event.angleDelta.y

                const kDefaultRotationStep = 120
                let newColumnsCountUserModifier =
                    Math.floor(logicalRotation / kDefaultRotationStep)

                const minModifier = 1 - sizesCalculator.defaultColumnsCount
                const kMaxModifier = 0

                newColumnsCountUserModifier =
                    MathUtils.bound(minModifier, newColumnsCountUserModifier, kMaxModifier)

                logicalRotation = newColumnsCountUserModifier * kDefaultRotationStep
                transitionOverlay.animateWheelLayoutChange(newColumnsCountUserModifier)
            }
        }
    }

    TransitionOverlay
    {
        id: transitionOverlay

        anchors.fill: parent

        onUserDefinedColumnsCountChanged: control.saveTargetColumnsCount()
    }

    Rectangle
    {
        id: topShadow

        readonly property real kMaxOpacity: 1
        readonly property int kAnimationZoneHeight: sizesCalculator.kMinCellHeight

        readonly property real animationFactor:
            MathUtils.bound(0, flickable.contentY / kAnimationZoneHeight, 1)

        width: parent.width
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

        // +1 is added, because for an unknown reason, 1px gap appears otherwise,
        // for some window size.
        y: flickable.height - height + 1

        width: parent.width
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
        target: null
        acceptedDevices: PointerDevice.TouchScreen
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        minimumPointCount: 2
        maximumPointCount: 2

        onActiveChanged:
        {
            if (!active)
                transitionOverlay.finalizePinchWithAnimation()
        }

        onActiveScaleChanged:
        {
            if (!active)
                return

            transitionOverlay.animatePinchLayoutChange(activeScale)
        }

        onGrabChanged: (transition) =>
        {
            flickable.interactive =
                transition !== PointerDevice.GrabExclusive
                    && transition !== PointerDevice.GrabPassive
        }
    }

    SizesCalculator
    {
        id: sizesCalculator

        cellsCount: repeater.count
        availableWidth: control.width - control.leftMargin - control.rightMargin
        availableHeight: control.height - control.topMargin - control.bottomMargin
        spacing: flow.spacing
    }

    PaddingsCalculator
    {
        id: paddingsCalculator

        availableWidth: sizesCalculator.availableWidth
        cellsCount: repeater.count
        columnsCount: sizesCalculator.columnsCount
        normalCellWidth: sizesCalculator.normalCellWidth
        enlargedCellsCount: sizesCalculator.enlargedCellsCount
        enlargedCellWidth: sizesCalculator.enlargedCellWidth
        spacing: sizesCalculator.spacing
    }

    RowIndexChecker
    {
        id: rowIndexChecker

        cellsCount: repeater.count
        enlargedCellsCount: sizesCalculator.enlargedCellsCount
    }
}
