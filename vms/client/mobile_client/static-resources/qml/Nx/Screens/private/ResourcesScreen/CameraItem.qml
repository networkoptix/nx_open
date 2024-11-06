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

    signal clicked()
    signal thumbnailRefreshRequested()

    padding: 4
    implicitWidth: 200
    implicitHeight: contentItem.implicitHeight + 2 * padding

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

        // This property prevents video component re-creation while scrolling.
        property bool videoAllowed: false
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
        color: ColorTheme.colors.windowBackground
    }

    contentItem: Column
    {
        id: contentColumn

        spacing: 8
        width: parent.availableWidth
        anchors.centerIn: parent

        Rectangle
        {
            id: thumbnailContainer

            width: parent.width
            height: parent.width * 9 / 16
            color: d.showTumbnailDummy ? ColorTheme.colors.dark7 : ColorTheme.colors.dark4

            Loader
            {
                id: thumbnailContentLoader

                anchors.centerIn: parent
                sourceComponent:
                {
                    if (d.showTumbnailDummy)
                        return thumbnailDummyComponent

                    if (!cameraItem.active || !d.videoAllowed || !settings.liveVideoPreviews)
                    {
                        if (!cameraItem.thumbnail)
                            return thumbnailPreloaderComponent

                        return thumbnailComponent
                    }

                    return videoComponent
                }
            }
        }

        Row
        {
            width: parent.width
            spacing: 6

            StatusIndicator
            {
                id: statusIndicator

                status: d.status
                y: 4
            }

            Text
            {
                id: label

                width: parent.width - x - parent.spacing
                height: 24
                font.pixelSize: 16
                font.weight: d.showTumbnailDummy ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                color: d.showTumbnailDummy ? ColorTheme.colors.dark11 : ColorTheme.windowText
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
        id: thumbnailDummyComponent

        Column
        {
            width: thumbnailContainer.width
            leftPadding: 8
            rightPadding: 8
            property real availableWidth: width - leftPadding - rightPadding

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
                color: ColorTheme.windowText
            }
        }
    }

    Component
    {
        id: thumbnailPreloaderComponent

        NxDotPreloader {}
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
                sourceSize: Qt.size(parent.width, parent.height)
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
                    target: uiController
                    function onCurrentScreenChanged()
                    {
                        if (uiController.currentScreen === Controller.ResourcesScreen)
                            mediaPlayer.playLive()
                    }
                }

                mediaPlayer: MediaPlayer
                {
                    id: mediaPlayer

                    resource: cameraItem.resource
                    videoQuality: mediaResourceHelper.livePreviewVideoQuality
                    audioEnabled: false
                    allowHardwareAcceleration: settings.enableHardwareDecoding
                    maxTextureSize: textureSizeHelper.maxTextureSize

                    Component.onCompleted:
                    {
                        cameraItem.mediaPlayer = this
                        playLive()
                    }
                }

                resourceHelper: mediaResourceHelper
            }

            Watermark
            {
                parent: video.videoOutput
                anchors.fill: parent

                sourceSize: Qt.size(parent.width, parent.height)
            }

            Image
            {
                id: image

                anchors.fill: parent
                source: mediaPlayer.audioOnlyMode
                    ? lp("/images/alert_sound.png")
                    : cameraItem.thumbnail
                fillMode: Qt.KeepAspectRatio
                visible: !video.visible && status === Image.Ready
            }

            NxDotPreloader
            {
                anchors.centerIn: parent
                visible: !video.visible && !image.visible && !mediaPlayer.audioOnlyMode
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
        running: active && sessionManager.hasConnectedSession

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

    Timer
    {
        interval: 2000
        running: active
        onTriggered: d.videoAllowed = true
    }
}
