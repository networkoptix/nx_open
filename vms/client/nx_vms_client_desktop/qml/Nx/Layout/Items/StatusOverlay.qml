// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

Item
{
    id: overlay

    property alias resource: helper.resource
    property MediaPlayer mediaPlayer: null

    enum OverlayType
    {
        EmptyOverlay,
        PausedOverlay,
        LoadingOverlay,
        NoDataOverlay,
        NoVideoDataOverlay,
        UnauthorizedOverlay,
        OfflineOverlay,
        AnalogWithoutLicenseOverlay,
        VideowallWithoutLicenseOverlay,
        ServerOfflineOverlay,
        ServerUnauthorizedOverlay,
        IoModuleDisabledOverlay,
        TooManyOpenedConnectionsOverlay,
        PasswordRequiredOverlay,
        NoLiveStreamOverlay,
        OldFirmwareOverlay,
        CannotDecryptMediaOverlay
    }

    readonly property alias overlayType: d.overlayType
    readonly property bool isError: d.overlayData.isError || false

    ResourceStatusHelper
    {
        id: helper
        context: windowContext
    }

    ScaleAdjuster
    {
        id: mainArea

        readonly property real availableHeight: d.buttonsVisible
            ? overlay.height - buttonsArea.height
            : overlay.height

        resizeToContent: true
        maximumWidth: overlay.width
        maximumHeight: overlay.height
        x: (overlay.width - width) / 2
        y: (availableHeight - height) / 2

        contentItem: Control
        {
            id: mainControls

            topPadding: 60
            leftPadding: 16
            rightPadding: 16
            bottomPadding: 60

            contentItem: Item
            {
                id: mainContent

                implicitWidth: Math.max(mainLayout.implicitWidth, preloader.implicitWidth)
                implicitHeight: Math.max(mainLayout.implicitHeight, preloader.implicitHeight)

                NxDotPreloader
                {
                    id: preloader

                    anchors.centerIn: mainContent
                    color: ColorTheme.colors.light13
                    borderColor: ColorTheme.colors.dark4
                    borderWidth: 1
                    padding: 64
                    spacing: 8
                    dotRadius: 8
                    visible: d.overlayType == StatusOverlay.LoadingOverlay
                }

                ColumnLayout
                {
                    id: mainLayout

                    spacing: 0
                    anchors.centerIn: mainContent

                    Image
                    {
                        id: iconImage

                        source: d.overlayData.icon || ""
                        Layout.alignment: Qt.AlignHCenter
                        visible: !!source.toString()
                    }

                    Text
                    {
                        id: captionText

                        text: d.overlayData.caption || ""

                        Layout.alignment: Qt.AlignHCenter
                        Layout.minimumWidth: d.minimumLayoutWidth

                        font.capitalization: Font.AllUppercase
                        font.pixelSize: overlay.isError ? 88 : 80
                        font.weight: Font.Light
                        horizontalAlignment: Qt.AlignHCenter
                        opacity: 0.7

                        color: overlay.isError
                            ? ColorTheme.colors.red_core
                            : ColorTheme.colors.light7

                        visible: !!text
                    }

                    Text
                    {
                        id: descriptionText

                        text: d.overlayData.description || ""

                        Layout.alignment: Qt.AlignHCenter
                        Layout.minimumWidth: d.minimumLayoutWidth
                        Layout.maximumWidth: d.minimumLayoutWidth

                        font.pixelSize: 36
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Qt.AlignHCenter
                        color: captionText.color
                        opacity: captionText.opacity

                        visible: !!text
                    }
                }
            }
        }
    }

    ScaleAdjuster
    {
        id: buttonsArea

        resizeToContent: true
        maximumWidth: overlay.height
        maximumHeight: overlay.height
        x: (overlay.width - width) / 2
        y: overlay.height - height
        visible: d.buttonsVisible

        contentItem: Control
        {
            id: buttonControls

            topPadding: 0
            leftPadding: 16
            rightPadding: 16
            bottomPadding: 24

            contentItem: ColumnLayout
            {
                spacing: 8

                Repeater
                {
                    id: buttonGenerator

                    model: d.overlayActions

                    Button
                    {
                        id: buttonDelegate

                        text: d.actionText(modelData)
                        Layout.alignment: Qt.AlignHCenter
                        visible: !!text

                        readonly property color baseColor: ColorTheme.colors.light14
                        backgroundColor: ColorTheme.transparent(baseColor, 0.15)
                        hoveredColor: ColorTheme.transparent(baseColor, 0.2)
                        pressedColor: ColorTheme.transparent(baseColor, 0.15)
                        outlineColor: "transparent"

                        textColor: hovered && !pressed
                            ? baseColor
                            : ColorTheme.transparent(baseColor, 0.8)

                        font.pixelSize: 13
                        font.weight: Font.Medium

                        implicitHeight: 32

                        onClicked:
                            helper.executeAction(modelData)
                    }
                }
            }
        }
    }

    QtObject
    {
        id: d

        readonly property bool buttonsVisible: overlayActions.length
            && (mainArea.height + buttonsArea.height) <= overlay.height
            && buttonControls.scale >= 0.5

        // TODO: #vkutin New scene overlays seem stronger downscaled than old scene ones,
        // so figure out why and adjust these limits if needed.
        readonly property real minimumLayoutWidth: overlay.isError ? 960 : 800

        readonly property int overlayType:
        {
            if (helper.status & ResourceStatusHelper.StatusFlag.videowallLicenseRequired)
                return StatusOverlay.VideowallWithoutLicenseOverlay

            if (helper.status & ResourceStatusHelper.StatusFlag.accessDenied)
                return StatusOverlay.AccessDenied

            if (helper.resourceType == ResourceStatusHelper.ResourceType.localMedia)
            {
                if (helper.status & ResourceStatusHelper.StatusFlag.offline)
                    return StatusOverlay.NoDataOverlay

                if (helper.status & ResourceStatusHelper.StatusFlag.noVideoData)
                    return StatusOverlay.NoVideoDataOverlay

                return StatusOverlay.EmptyOverlay
            }

            if (helper.status & ResourceStatusHelper.StatusFlag.oldFirmware)
                return StatusOverlay.OldFirmwareOverlay

            const playingLive = overlay.mediaPlayer && overlay.mediaPlayer.liveMode
            if (playingLive)
            {
                if (helper.status & ResourceStatusHelper.StatusFlag.noLiveStream)
                    return StatusOverlay.NoLiveStreamOverlay

                if (helper.status & ResourceStatusHelper.StatusFlag.offline)
                    return StatusOverlay.OfflineOverlay

                if (helper.status & ResourceStatusHelper.StatusFlag.unauthorized)
                    return StatusOverlay.UnauthorizedOverlay

                if (helper.status & ResourceStatusHelper.StatusFlag.defaultPassword)
                    return StatusOverlay.PasswordRequiredOverlay
            }

            if ((helper.status & ResourceStatusHelper.StatusFlag.dts)
                && helper.licenseUsage == ResourceStatusHelper.LicenseUsage.notUsed)
            {
                if ((!playingLive
                        && !(helper.status & ResourceStatusHelper.StatusFlag.canViewArchive))
                    || (playingLive
                        && !(helper.status & ResourceStatusHelper.StatusFlag.canViewLive)))
                {
                    return StatusOverlay.AnalogWithoutLicenseOverlay
                }
            }

            if (helper.resourceType == ResourceStatusHelper.ResourceType.ioModule)
            {
                if (!playingLive)
                {
                 // TODO: FIXME! #vkutin Handle no data state.
                    return StatusOverlay.NoVideoDataOverlay
                }

                return (helper.licenseUsage != ResourceStatusHelper.LicenseUsage.used)
                    ? StatusOverlay.IoModuleDisabledOverlay
                    : StatusOverlay.EmptyOverlay
            }

            if (overlay.mediaPlayer && overlay.mediaPlayer.tooManyConnectionsError)
                return StatusOverlay.TooManyOpenedConnectionsOverlay
            if (overlay.mediaPlayer && overlay.mediaPlayer.cannotDecryptMediaError)
                return StatusOverlay.CannotDecryptMediaOverlay

            if (mediaPlayer && mediaPlayer.mediaStatus == MediaPlayer.MediaStatus.Loading)
            {
                // TODO: FIXME! #vkutin Handle no data state.
                return StatusOverlay.LoadingOverlay
            }

            return StatusOverlay.EmptyOverlay
        }

        function actionText(actionType)
        {
            switch (actionType)
            {
                case ResourceStatusHelper.ActionType.diagnostics:
                    return qsTr("Diagnostics")

                case ResourceStatusHelper.ActionType.enableLicense:
                    return qsTr("Enable")

                case ResourceStatusHelper.ActionType.moreLicenses:
                    return qsTr("Activate License")

                case ResourceStatusHelper.ActionType.settings:
                {
                    switch (helper.resourceType)
                    {
                        case ResourceStatusHelper.ResourceType.camera:
                            return qsTr("Camera Settings")

                        case ResourceStatusHelper.ResourceType.ioModule:
                            return qsTr("I/O Module Settings")

                        default:
                            return qsTr("Device Settings")
                    }
                }

                case ResourceStatusHelper.ActionType.setPassword:
                    return qsTr("Set for this Camera")

                case ResourceStatusHelper.ActionType.setPasswords:
                    return qsTr("Set for all %n Cameras", "", helper.defaultPasswordCameras)

                default:
                    return ""
            }
        }

        readonly property var overlayData:
        {
            switch (overlayType)
            {
                case StatusOverlay.NoDataOverlay:
                    return makeOverlayData(qsTr("No data"))

                case StatusOverlay.UnauthorizedOverlay:
                    return makeErrorOverlayData(qsTr("Unauthorized"),
                        "qrc:///skin/item_placeholders/unauthorized.png",
                        qsTr("Please check authentication information"))

                case StatusOverlay.AccessDenied:
                    return makeErrorOverlayData(qsTr("No access"),
                        "image://svg/skin/item_placeholders/no_access.svg")

                case StatusOverlay.OfflineOverlay:
                    return makeErrorOverlayData(qsTr("No signal"),
                        "image://svg/skin/item_placeholders/offline.svg")

                case StatusOverlay.AnalogWithoutLicenseOverlay:
                case StatusOverlay.VideowallWithoutLicenseOverlay:
                    return makeErrorOverlayData(qsTr("Not enough licenses"),
                        "qrc:///skin/item_placeholders/license.png")

                case StatusOverlay.ServerOfflineOverlay:
                    return makeErrorOverlayData(qsTr("Server unavailable"))

                case StatusOverlay.ServerUnauthorizedOverlay:
                    return makeErrorOverlayData(qsTr("No access"),
                        "qrc:///skin/item_placeholders/no_access.svg")

                case StatusOverlay.IoModuleDisabledOverlay:
                    return makeErrorOverlayData(qsTr("Device disabled"),
                        "qrc:///skin/item_placeholders/disabled.png")

                case StatusOverlay.TooManyOpenedConnectionsOverlay:
                    return makeErrorOverlayData(qsTr("Too many connections"))

                case StatusOverlay.CannotDecryptMediaOverlay:
                    return makeErrorOverlayData(qsTr("Cannot decrypt media"))

                case StatusOverlay.PasswordRequiredOverlay:
                    return makeErrorOverlayData(qsTr("Password required"),
                        "qrc:///skin/item_placeholders/alert.png")

                case StatusOverlay.NoLiveStreamOverlay:
                    return makeOverlayData(qsTr("No live stream"))

                case StatusOverlay.OldFirmwareOverlay:
                    return makeErrorOverlayData(qsTr("Unsupported firmware version"))

                case StatusOverlay.NoVideoDataOverlay:
                    return makeIconOverlayData("qrc:///skin/item_placeholders/sound.png")

                case StatusOverlay.PausedOverlay:
                    return makeIconOverlayData("qrc:///skin/item_placeholders/pause.png")

                default:
                    return makeOverlayData()
            }
        }

        readonly property var overlayActions:
        {
            switch (overlayType)
            {
                case StatusOverlay.PasswordRequiredOverlay:
                {
                    if (!(helper.status & ResourceStatusHelper.StatusFlag.canChangePasswords))
                        return []

                    let actions = [ResourceStatusHelper.ActionType.setPassword]
                    if (helper.camerasWithDefaultPassword > 1)
                        actions.push(ResourceStatusHelper.ActionType.setPasswords)

                    return actions
                }

                case StatusOverlay.IoModuleDisabledOverlay:
                case StatusOverlay.AnalogWithoutLicenseOverlay:
                {
                    if (!(helper.status & ResourceStatusHelper.StatusFlag.canEditSettings))
                        return []

                    switch (helper.licenseUsage)
                    {
                        case ResourceStatusHelper.LicenseUsage.notUsed:
                            return [ResourceStatusHelper.ActionType.enableLicense]

                        case ResourceStatusHelper.LicenseUsage.overflow:
                            return [ResourceStatusHelper.ActionType.moreLicenses]

                        default:
                            return []
                    }
                }

                case StatusOverlay.UnauthorizedOverlay:
                {
                    return (helper.status & ResourceStatusHelper.StatusFlag.canEditSettings)
                        ? [ResourceStatusHelper.ActionType.settings]
                        : []
                }

                case StatusOverlay.AccessDenied:
                case StatusOverlay.OldFirmwareOverlay:
                case StatusOverlay.OfflineOverlay:
                {
                    return (helper.status & ResourceStatusHelper.StatusFlag.canInvokeDiagnostics)
                        ? [ResourceStatusHelper.ActionType.diagnostics]
                        : []
                }

                default:
                    return []
            }
        }

        function makeOverlayData(caption, icon, description, isError)
        {
            return {
                "caption": caption,
                "description": description,
                "icon": icon,
                "isError": isError}
        }

        function makeErrorOverlayData(caption, icon, description)
        {
            return makeOverlayData(caption, icon, description, /*isError*/ true)
        }

        function makeIconOverlayData(icon)
        {
            return makeOverlayData(undefined, icon)
        }
    }
}
