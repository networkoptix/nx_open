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

    readonly property bool fisheyeMode: fisheyeModeController.fisheyeMode

    property alias mediaPlayer: videoContent.mediaPlayer
    property alias resourceHelper: videoContent.resourceHelper
    property alias motionController: motionSearchController

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

    allowCompositeEvents: !motionController.drawingRoi && !fisheyeMode
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
            !motionController.motionSearchMode
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
        parent: videoContent.videoOutput
        anchors.fill: parent

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

        MotionController
        {
            id: motionSearchController

            dragThreshold: control.mouseArea.drag.threshold
            parent: videoContent.videoOutput
            anchors.fill: parent
            viewport: control

            allowDrawing: !control.fisheyeMode
            cameraRotation: control.resourceHelper.customRotation
            motionProvider.mediaPlayer: videoContent.mediaPlayer

            Connections
            {
                target: control.mouseArea
                enabled: motionSearchController.enabled

                function onPressed(mouse)
                {
                    motionSearchController.handlePressed(
                        control.mapToItem(motionSearchController, mouse.x, mouse.y))
                }

                function onReleased()
                {
                    motionSearchController.handleReleased()
                }

                function onPositionChanged(mouse)
                {
                    motionSearchController.handlePositionChanged(
                        control.mapToItem(motionSearchController, mouse.x, mouse.y))
                }

                function onCanceled()
                {
                    motionSearchController.handleCancelled()
                }

                function onDoubleClicked()
                {
                    motionSearchController.handleCancelled()
                }
            }

            Connections
            {
                target: control

                function onMovementEnded()
                {
                    motionSearchController.updateDefaultRoi()
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

        property bool showRoiHint:
            motionController.motionSearchMode && !motionController.drawingRoi && falseDragging

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
