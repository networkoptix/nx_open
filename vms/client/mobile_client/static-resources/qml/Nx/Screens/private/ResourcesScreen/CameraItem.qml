// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core
import Nx.Core.Items
import Nx.Core.Controls
import Nx.Items
import Nx.Mobile
import Nx.Screens
import Nx.Settings

import nx.vms.api
import nx.vms.client.mobile
import nx.vms.client.core

import ".."

Control
{
    id: cameraItem

    property alias text: label.text
    property string resourceName
    property string thumbnail
    property int status
    property bool keepStatus: false
    property alias resource: mediaResourceHelper.resource
    property var mediaPlayer
    property bool active: false
    property real aspectRatio: 9 / 16

    signal clicked()
    signal thumbnailRefreshRequested()

    SimpleVideoOverlayController
    {
        id: d

        status: API.ResourceStatus.offline //< The status is handled via custom logic.
        mediaResourceHelper: mediaResourceHelper

        readonly property bool showThumbnailDummy:
            dummyState !== "" || status === API.ResourceStatus.undefined
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper
    }

    TextureSizeHelper
    {
        id: textureSizeHelper
    }

    Binding
    {
        target: d
        property: "status"
        value: status
        when: !keepStatus
        restoreMode: Binding.RestoreBinding
    }

    background: Rectangle
    {
        color: ColorTheme.colors.dark4
    }

    contentItem: Item
    {
        id: itemHolder

        Rectangle
        {
            id: thumbnailContainer

            width: parent.width
            height: Math.floor(width * cameraItem.aspectRatio)

            color: d.showThumbnailDummy ? ColorTheme.colors.dark8 : ColorTheme.colors.dark4

            Item
            {
                id: thumbnailContent

                width: parent.width
                height: parent.width * cameraItem.aspectRatio

                Loader
                {
                    id: thumbnailContentLoader

                    anchors.centerIn: parent
                    state:
                    {
                        if (d.showThumbnailDummy)
                            return "dummyContent"

                        const allowVideoContent = !initialLoadingTimer.running
                            && cameraItem.active
                            && appContext.settings.liveVideoPreviews

                        return allowVideoContent
                            ? "videoContent"
                            : "thumbnailContent"
                    }

                    sourceComponent:
                    {
                        switch (state)
                        {
                            case "thumbnailContent":
                                return thumbnailComponent
                            case "videoContent":
                                return videoComponent
                            default:
                                return dummyComponent
                        }
                    }
                }
            }
        }

        Rectangle
        {
            id: cameraInfo

            readonly property int margin: 4
            readonly property int padding: 6
            readonly property int spacing: 2

            radius: 3
            color: ColorTheme.transparent(ColorTheme.colors.dark6, 0.6)

            x: margin
            y: parent.height - height - margin
            height: 26

            width:
            {
                const indicatorSpace = statusIndicator.width
                    ? statusIndicator.width + spacing
                    : 0

                return indicatorSpace + label.width + padding * 2
            }

            RecordingStatusIndicator
            {
                id: statusIndicator

                x: cameraInfo.padding
                anchors.verticalCenter: parent.verticalCenter

                useSmallIcon: true
                resource: cameraItem.resource
                sourceSize: Qt.size(10, 10)
            }

            Text
            {
                id: label

                x: statusIndicator.width
                   ? statusIndicator.x + statusIndicator.width + cameraInfo.spacing
                   : cameraInfo.padding
                width:
                {
                    const maxWidth =
                        itemHolder.width - label.x - cameraInfo.padding - cameraInfo.margin * 2
                    return Math.min(implicitWidth, maxWidth)
                }

                text: (!!mediaResourceHelper.crossSystemName ? "../" : "") + cameraItem.resourceName
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 12
                elide: Text.ElideRight
                color: ColorTheme.colors.light4
            }
        }
    }

    TapHandler
    {
        id: tapHandler
        onSingleTapped: cameraItem.clicked()
    }

    MaterialEffect
    {
        clip: true
        anchors.fill: parent
        mouseArea: tapHandler
        rippleSize: width * 2
    }

    Component
    {
        id: dummyComponent

        VideoDummy
        {
            width: thumbnailContent.width
            height: thumbnailContent.height

            state: d.dummyState

            onLogInClicked: mediaResourceHelper.cloudAuthorize()
        }
    }

    Component
    {
        id: thumbnailComponent

        Image
        {
            source: cameraItem.thumbnail
            width: thumbnailContainer.width
            height: thumbnailContainer.height
            fillMode: Qt.KeepAspectRatio

            Watermark
            {
                anchors.fill: parent
                resource: cameraItem.resource
                sourceSize: Qt.size(parent.width, parent.height)
            }

            NxDotPreloader
            {
                anchors.centerIn: parent
                visible: !cameraItem.thumbnail
            }
        }
    }

    Component
    {
        id: videoComponent

        Item
        {
            width: thumbnailContainer.width
            height: thumbnailContainer.height

            MultiVideoPositioner
            {
                id: video

                anchors.fill: parent
                visible: mediaPlayer.playing


                Connections
                {
                    target: windowContext.deprecatedUiController
                    function onCurrentScreenChanged()
                    {
                        if (windowContext.deprecatedUiController.currentScreen
                            === Controller.ResourcesScreen)
                        {
                            mediaPlayer.playLive()
                        }
                    }
                }

                mediaPlayer: MediaPlayer
                {
                    id: mediaPlayer

                    resource: cameraItem.resource
                    videoQuality: mediaResourceHelper.livePreviewVideoQuality
                    audioEnabled: false
                    allowHardwareAcceleration: appContext.settings.enableHardwareDecoding
                    maxTextureSize: textureSizeHelper.maxTextureSize

                    Component.onCompleted:
                    {
                        cameraItem.mediaPlayer = this

                        // Qt.callLater defers playLive() to the next event loop iteration.
                        // This is necessary because QVideoSink::rhi() - the RHI context used
                        // to select a hardware video decoder (e.g. VideoToolbox, MediaCodec) -
                        // is set by the Qt scene graph on the render thread during its first
                        // render pass. At the time Component.onCompleted fires, the VideoOutput
                        // inside MultiVideoOutput has just been created but not yet rendered,
                        // so rhi() is still null. Calling playLive() directly here would cause
                        // the hardware decoder to be skipped and the stream to fall back to
                        // software decoding. Deferring by one event loop iteration gives the
                        // scene graph time to perform its first render pass and initialize rhi().
                        Qt.callLater(playLive)
                    }
                }

                resourceHelper: mediaResourceHelper
            }

            Image
            {
                id: videoThumbnail

                anchors.fill: parent
                source: cameraItem.thumbnail
                fillMode: Qt.KeepAspectRatio
                visible: !video.visible && status === Image.Ready && !mediaPlayer.audioOnlyMode
            }

            VideoDummy
            {
                anchors.fill: parent
                state: "audioOnlyMode"
                visible: !video.visible && mediaPlayer.audioOnlyMode
            }

            Watermark
            {
                parent: video.videoOutput
                anchors.fill: parent

                resource: cameraItem.resource
                sourceSize: Qt.size(parent.width, parent.height)
            }

            NxDotPreloader
            {
                anchors.centerIn: parent
                visible:!video.visible
                    && !videoThumbnail.visible
                    && !mediaPlayer.audioOnlyMode
            }
        }
    }

    Timer
    {
        id: refreshTimer

        readonly property int initialLoadDelay: 400
        readonly property int reloadDelay: 60 * 1000

        interval: initialLoadDelay
        repeat: true
        running: active
            && windowContext.sessionManager.hasConnectedSession
            && thumbnailContentLoader.state !== "dummyContent"

        onTriggered:
        {
            interval = reloadDelay
            cameraItem.thumbnailRefreshRequested()
        }

        onRunningChanged:
        {
            if (!running)
                interval = initialLoadDelay
        }
    }

    // Avoid excessive video components and thumbnail requests creation while scrolling.
    Timer
    {
        id: initialLoadingTimer

        repeat: false
        running: true
        interval: 2000
    }
}
