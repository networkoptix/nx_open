// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtMultimedia

import nx.vms.client.core

import Nx.Controls
import Nx.Core.Items
import Nx.Core.Controls
import Nx.Items
import Nx.Core

ScalableContentHolder
{
    id: control

    readonly property alias fisheyeMode: fisheyeModeController.fisheyeMode

    property bool showMotion: false

    property alias mediaPlayer: videoContent.mediaPlayer
    property alias resourceHelper: videoContent.resourceHelper
    property alias roiController: roiController

    property alias videoCenterHeightOffsetFactor: videoContent.videoCenterHeightOffsetFactor
    property size fitSize: videoContent.boundedSize(width, height)

    signal showRoiHint()
    signal hideRoiHint()

    interactive: !fisheyeMode
    content: videoContent

    function getMoveViewportData(position)
    {
        var videoItem = videoContent.videoOutput
        var mapped = mapToItem(videoItem, position.x, position.y)
        return videoItem.getMoveViewportData(mapped)
    }

    Connections
    {
        target: resourceHelper
        function onResourceChanged()
        {
            to1xScale()
        }
    }

    allowCompositeEvents: !roiController.drawingRoi && !fisheyeMode
    minContentWidth: width
    minContentHeight: height
    maxContentWidth:
        maxZoomFactor * Math.max(
            videoContent.rotated90
                ? videoContent.sourceSize.height
                : videoContent.sourceSize.width,
            width)
    maxContentHeight:
        maxZoomFactor * Math.max(
            videoContent.rotated90
                ? videoContent.sourceSize.width
                : videoContent.sourceSize.height,
            height)

    clip: true

    doubleTapStartCheckFuncion:
        function(initialPosition)
        {
            var videoItem = videoContent.videoOutput
            var videoMappedPosition = mapToItem(videoItem, initialPosition.x, initialPosition.y)
            return videoItem.pointInVideo(videoMappedPosition);
        }

    NxObject
    {
        id: fisheyeModeController

        property bool fisheyeModeInternal:
            !roiController.motionSearchMode
            && resourceHelper.fisheyeParams.enabled

        property bool fisheyeMode: false
        Component.onCompleted: fisheyeMode = fisheyeModeInternal

        Connections
        {
            target: control
            function onMovingChanged()
            {
                if (!moving && fisheyeModeController.fisheyeModeInternal)
                    fisheyeModeController.fisheyeMode = true
            }
        }

        Connections
        {
            target: interactor
            function onAnimatingChanged()
            {
                if (!interactor.animating && !fisheyeModeController.fisheyeModeInternal)
                    fisheyeModeController.fisheyeMode = false
            }
        }

        onFisheyeModeInternalChanged:
        {
            if (fisheyeModeInternal)
            {
                // Try to rollback scalable video picture
                if (control.contentWidth > control.width || control.contentHeight > control.height)
                    control.fitToBounds(true)
                else
                    fisheyeMode = true
            }
            else
            {
                // Rollback fisheye video picture
                if (interactor.scalePower != 0 || interactor.unconstrainedRotation != Qt.vector2d(0, 0))
                {
                    interactor.enableAnimation = true
                    interactor.scalePower = 0
                    interactor.unconstrainedRotation = Qt.vector2d(0, 0)
                    interactor.enableAnimation = false
                }
                else
                {
                    fisheyeMode = false
                }
            }
        }
    }

    FisheyeShaderItem
    {
        id: shader

        visible: control.fisheyeMode
        // If sourceItem size is 0, shader item may show rendering artifacts,
        sourceItem: visible && videoContent.videoOutput.channelSize.width > 0
            ? videoContent.videoOutput
            : null
        helper: control.resourceHelper
        anchors.fill: videoContent
        videoCenterHeightOffsetFactor: control.videoCenterHeightOffsetFactor
    }

    FisheyeInteractor
    {
        id: interactor

        fisheyeShader: shader
        enableAnimation: fisheyeController.enableAnimation
    }

    FisheyeController
    {
        id: fisheyeController

        enabled: control.fisheyeMode
        interactor: interactor
        mouseArea: control.mouseArea
        pinchArea: control.pinchArea
        onClicked: control.clicked()
    }

    Watermark
    {
        id: watermark

        parent: videoContent.videoOutput
        anchors.fill: parent

        resource: control.resourceHelper.resource
        sourceSize: videoContent.videoOutput.channelSize
    }

    MultiVideoPositioner
    {
        id: videoContent

        objectName: "test"
        resourceHelper: control.resourceHelper

        width: contentWidth
        height: contentHeight

        onSourceSizeChanged: fitToBounds()

        RoundedMask
        {
            id: motionDisplay

            readonly property bool showMotion: control.showMotion && !control.fisheyeMode

            visible: motionDisplay.showMotion
            parent: videoContent.videoOutput

            anchors.fill: parent

            opacity: 1
            cellCountX: 44
            cellCountY: 32
            fillColor: ColorTheme.transparent(ColorTheme.colors.red_l2, 0.15)
            lineColor: ColorTheme.transparent(ColorTheme.colors.red_l2, 0.5)

            MediaPlayerMotionProvider
            {
                id: mediaPlayerMotionProvider

                mediaPlayer: motionDisplay.showMotion ? videoContent.mediaPlayer : null

                onMotionMaskChanged:
                    motionMaskItem.motionMask = mediaPlayerMotionProvider.motionMask(0)
            }

            maskTextureProvider: MotionMaskItem
            {
                id: motionMaskItem
            }
        }

        RoiController
        {
            id: roiController

            dragThreshold: control.mouseArea.drag.threshold
            parent: videoContent.videoOutput
            anchors.fill: parent
            viewport: control

            allowDrawing: !control.fisheyeMode
            cameraRotation: control.resourceHelper.customRotation

            Connections
            {
                target: control.mouseArea
                enabled: roiController.enabled

                function onPressed(mouse)
                {
                    roiController.handlePressed(
                        control.mapToItem(roiController, mouse.x, mouse.y))
                }

                function onReleased()
                {
                    roiController.handleReleased()
                }

                function onPositionChanged(mouse)
                {
                    roiController.handlePositionChanged(
                        control.mapToItem(roiController, mouse.x, mouse.y))
                }

                function onCanceled()
                {
                    roiController.handleCancelled()
                }

                function onDoubleClicked()
                {
                    roiController.handleCancelled()
                }
            }

            Connections
            {
                target: control

                function onMovementEnded()
                {
                    roiController.updateDefaultRoi()
                }
            }

            onDrawingRoiChanged:
            {
                if (drawingRoi)
                    control.hideRoiHint()
            }
        }
    }

    NxObject
    {
        id: d

        property bool showRoiHint: roiController.motionSearchMode
            && roiController.enabled
            && !roiController.drawingRoi
            && falseDragging

        Timer
        {
            id: roiHintFilterTimer
            interval: 200
            onTriggered:
            {
                if (d.showRoiHint)
                    control.showRoiHint()
            }
        }

        onShowRoiHintChanged: roiHintFilterTimer.restart()
    }
}
