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
import Nx.Settings

import nx.vms.api
import nx.vms.client.mobile
import nx.vms.client.core

Control
{
    id: cameraItem

    property alias text: label.text
    property string thumbnail
    property int status
    property bool keepStatus: false
    property alias resource: mediaResourceHelper.resource
    property var mediaPlayer
    property bool active: false
    property real aspectRatio: 9 / 16

    signal clicked()
    signal thumbnailRefreshRequested()

    QtObject
    {
        id: d

        property int status: { API.ResourceStatus.offline }

        readonly property bool offline: status == API.ResourceStatus.offline
        readonly property bool showTumbnailDummy: offline ||
                               status == API.ResourceStatus.undefined ||
                               status == API.ResourceStatus.unauthorized ||
                               hasDefaultPassword || hasOldFirmware || ioModule
        readonly property bool unauthorized: status == API.ResourceStatus.unauthorized
        readonly property bool hasDefaultPassword: mediaResourceHelper.hasDefaultCameraPassword
        readonly property bool hasOldFirmware: mediaResourceHelper.hasOldCameraFirmware
        readonly property bool ioModule: !mediaResourceHelper.hasVideo && mediaResourceHelper.isIoModule
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
            height: width * cameraItem.aspectRatio


            color: d.showTumbnailDummy ? ColorTheme.colors.dark8 : ColorTheme.colors.dark4

            Item
            {
                width: parent.width
                height:
                {
                    const dummyModeOffset = d.showTumbnailDummy
                        ? cameraInfo.height + cameraInfo.margin * 2
                        : 0
                    return parent.width * cameraItem.aspectRatio - dummyModeOffset
                }

                Loader
                {
                    id: thumbnailContentLoader

                    anchors.centerIn: parent
                    state:
                    {
                        if (d.showTumbnailDummy)
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

                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 12
                elide: Text.ElideRight
                color: ColorTheme.colors.light4
            }
        }
    }

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        onClicked: cameraItem.clicked()
    }

    MaterialEffect
    {
        clip: true
        anchors.fill: parent
        mouseArea: mouseArea
        rippleSize: width * 2
    }

    Component
    {
        id: dummyComponent

        Column
        {
            width: thumbnailContainer.width
            leftPadding: 8
            rightPadding: 8
            property real availableWidth: width - leftPadding - rightPadding

            scale: thumbnailContainer.height < height
                ? thumbnailContainer.height / height
                : 1

            Image
            {
                anchors.horizontalCenter: parent.horizontalCenter
                source:
                {
                    if (d.unauthorized)
                        return lp("/images/camera_locked.png")
                    if (d.hasDefaultPassword || d.hasOldFirmware)
                        return lp("/images/camera_alert.png")
                    if (d.offline)
                        return lp("/images/camera_offline.png")
                    if (d.ioModule)
                        return lp("/images/preview_io.svg")
                    return ""
                }
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.availableWidth - 32
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                text:
                {
                    if (d.unauthorized)
                        return qsTr("Authentication required")
                    if (d.hasDefaultPassword)
                        return qsTr("Password required")
                    if (d.hasOldFirmware)
                        return qsTr("Unsupported firmware version")
                    if (d.offline)
                        return qsTr("Offline")
                    if (d.ioModule)
                        return qsTr("I/O module")
                    return ""
                }

                wrapMode: Text.WordWrap
                maximumLineCount: 2
                font.pixelSize: 14
                font.weight: Font.Normal
                color: ColorTheme.colors.light1
            }
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
                    target: windowContext.depricatedUiController
                    function onCurrentScreenChanged()
                    {
                        if (windowContext.depricatedUiController.currentScreen
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
                        playLive()
                    }
                }

                resourceHelper: mediaResourceHelper
            }

            Image
            {
                id: videoThumbnail

                anchors.fill: parent
                source: mediaPlayer.audioOnlyMode
                    ? lp("/images/alert_sound.png")
                    : cameraItem.thumbnail
                fillMode: Qt.KeepAspectRatio
                visible: !video.visible && status === Image.Ready
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
