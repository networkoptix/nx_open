// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls as MobileControls
import Nx.Mobile.Ui.Sheets
import Nx.Models
import Nx.Screens
import Nx.Settings
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

import "items"

Page
{
    id: eventDetailsScreen

    objectName: "eventDetailsScreen"

    property bool isAnalyticsDetails: true
    property QnCameraListModel camerasModel: null
    property alias eventSearchModel: d.accessor.model
    property int currentEventIndex: -1

    onLeftButtonClicked: Workflow.popCurrentScreen()

    title: qsTr("Preview")

    toolBar.visible: opacity > 0
    toolBar.opacity: d.hasControls ? 1 : 0

    clip: false

    rightControl: IconButton
    {
        id: shareButton

        padding: 0
        icon.source: backend.isShared && !eventDetailsScreen.isAnalyticsDetails
            ? "image://skin/20x20/Solid/shared.svg?primary=light10&secondary=green"
            : "image://skin/20x20/Solid/share.svg?primary=light10"
        icon.width: 24
        icon.height: icon.width

        visible: backend.isAvailable

        ShareBookmarkSheet
        {
            id: shareBookmarkSheet

            isAnalyticsItemMode: eventDetailsScreen.isAnalyticsDetails
            backend: ShareBookmarkBackend
            {
                id: backend

                modelIndex: eventSearchModel.index(currentEventIndex, 0)
            }

            onShowHowItWorks: howItWorksSheet.open()
        }

        HowItWorksSheet
        {
            id: howItWorksSheet

            description: qsTr("Sharing opens the new bookmark dialog to generate a playback link" +
                " after setting the sharing options")
            doNotShowAgain: !appContext.settings.showHowShareWorksNotification
            onContinued:
            {
                appContext.settings.showHowShareWorksNotification = !doNotShowAgain
                shareBookmarkSheet.showSheet()
            }
        }

        onClicked:
        {
            if (appContext.settings.showHowShareWorksNotification && isAnalyticsDetails)
                howItWorksSheet.open()
            else
                shareBookmarkSheet.showSheet()
        }
    }

    toolBar.contentItem.clip: false
    gradientToolbarBackground: true

    toolBar.background.opacity: LayoutController.isPortrait ? 0 : 1

    Rectangle
    {
        color: ColorTheme.colors.dark3
        anchors.fill: preview
    }

    AudioController
    {
        id: audioController

        resource: preview.resource
        serverSessionManager: windowContext.sessionManager
    }

    MediaPlaybackInterruptor
    {
        player: preview.player
        interruptOnInactivity: CoreUtils.isMobilePlatform()
    }

    IntervalPreview
    {
        id: preview

        audioEnabled: audioController.audioEnabled && eventDetailsScreen.activePage

        y: !LayoutController.isPortrait && header.visible
           ? -header.height
           : 0
        x: -windowParams.leftMargin

        width:
        {
            return LayoutController.isPortrait
                ? parent.width
                : mainWindow.width
        }

        height:
        {
            if (!LayoutController.isPortrait)
                return eventDetailsScreen.height

            const realWidth =
                function()
                {
                    if (!preview.isReady)
                        return width / 16 * 9

                    return videoRotation === 0 || videoRotation === 180
                        ? width / videoAspectRatio
                        : width * videoAspectRatio
                }()

            const maxHeight = (eventDetailsScreen.height - header.height - playbackPanel.height) / 2
            return Math.min(realWidth, maxHeight)
        }

        clip: true
        scalable: true
        hasPreloader: true
        aspectRatio: 0 //< No forced aspect ratio.
        active: true
        autoplay: false
        autoRepeat: false
        color: "transparent"

        onClicked: d.showControls = !d.showControls

        Image
        {
            y: parent.height - height + 1
            width: parent.width
            height: 56

            source: lp("/images/timeline_gradient.png")
            opacity: d.hasControls ? 1 : 0
            visible: LayoutController.isPortrait && opacity > 0
        }

        DownloadMediaButton
        {
            id: downloadButton

            parent: LayoutController.isPortrait
                ? preview
                : playbackPanel

            x: LayoutController.isPortrait
                ? parent.width - width - orientationModeButton.width - goToCameraButton.width
                : orientationModeButton.x - width
            y: LayoutController.isPortrait
                ? parent.height - height
                : (parent.height - height) / 2

            visible: isDownloadAvailable && !preview.cannotDecryptMedia
            enabled: preview.player && !preview.cannotDecryptMedia
            resource: preview.resource
            positionMs: preview.startTimeMs
            durationMs: preview.durationMs
        }

        IconButton
        {
            id: goToCameraButton

            visible: LayoutController.isPortrait && !preview.cannotDecryptMedia

            x: parent.width - width - orientationModeButton.width
            y: parent.height - height

            padding: 0
            icon.source: lp("/images/go_to_camera.svg")
            icon.width: 24
            icon.height: icon.width
            onClicked: d.goToCamera()
        }

        IconButton
        {
            id: orientationModeButton

            parent: LayoutController.isPortrait
                ? preview
                : playbackPanel

            x: LayoutController.isPortrait
                ? parent.width - width
                : parent.width - width - 16;
            y: LayoutController.isPortrait
                ? parent.height - height
                : (parent.height - height) / 2

            width: 48
            height: 48
            visible: !preview.cannotDecryptMedia

            icon.source: LayoutController.isPortrait
                ? lp("/images/fullscreen_view_mode.svg")
                : lp("/images/exit_fullscreen_mode.svg")
            icon.width: 24
            icon.height: icon.width
            padding: 0

            onClicked:
            {
                if (LayoutController.isPortrait)
                {
                    // Go to landscape mode.
                    if (CoreUtils.isMobilePlatform())
                        windowContext.ui.windowHelpers.setScreenOrientation(Qt.LandscapeOrientation)
                    else
                        [mainWindow.width, mainWindow.height] = [mainWindow.height, mainWindow.width]
                }
                else
                {
                    // Go to portrait mode.
                    if (CoreUtils.isMobilePlatform())
                        windowContext.ui.windowHelpers.setScreenOrientation(Qt.PortraitOrientation)
                    else
                        [mainWindow.width, mainWindow.height] = [mainWindow.height, mainWindow.width]
                }
            }
        }
    }

    VideoDummy
    {
        anchors.fill: preview
        visible: preview.cannotDecryptMedia
        state: "cannotDecryptMedia"
    }

    Flickable
    {
        width: parent.width

        anchors.top: preview.bottom
        anchors.topMargin: 14
        anchors.bottom: playbackPanel.top

        contentWidth: parent.width
        contentHeight: scrollableData.height

        clip: true

        Column
        {
            id: scrollableData

            width: parent.width - leftPadding - rightPadding
            leftPadding: 15
            rightPadding: 15
            spacing: 14

            Text
            {
                width: parent.width
                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light16
                font.pixelSize: 16
                font.weight: Font.Medium

                text: d.accessor.getData(currentEventIndex, "resource").name
            }

            MobileControls.TagView
            {
                width: parent.width
                model: d.accessor.getData(currentEventIndex, "tags")
                visible: hasTags
            }

            Text
            {
                id: titleText

                width: parent.width

                elide: Text.ElideRight
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 2

                color: ColorTheme.colors.light4
                font.pixelSize: 20
                font.weight: Font.Medium

                text: d.accessor.getData(currentEventIndex, "display")
            }

            Text
            {
                id: descriptionText

                width: parent.width
                color: ColorTheme.colors.light10
                font.pixelSize: 14
                font.weight: Font.Normal
                wrapMode: Text.Wrap
                text: d.accessor.getData(currentEventIndex, "description")
                visible: !!text

                onLinkActivated: (link) =>
                {
                    windowContext.ui.showLinkAboutToOpenMessage(link)
                }
            }

            AnalyticsAttributeTable
            {
                width: parent.width

                attributes: d.accessor.getData(currentEventIndex, "analyticsAttributes") ?? []

                nameFont.pixelSize: 16
                nameColor: ColorTheme.colors.light16
                valueFont.pixelSize: 16
                valueColor: ColorTheme.colors.light4
                valueFont.weight: Font.Medium
                colorBoxSize: 18
            }
        }
    }

    Rectangle
    {
        id: playbackPanel

        visible: opacity > 0
        opacity:
        {
            if (!d.hasControls)
                return 0
            return d.portrait ? 1 : 0.8
        }

        x: mainWindow.hasNavigationBar ? 0 : -windowParams.leftMargin
        y: parent.height - height
        width: mainWindow.hasNavigationBar ? parent.width : mainWindow.width
        height: 76

        color: ColorTheme.colors.dark1

        Button
        {
            labelPadding: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: !LayoutController.isPortrait

            icon.source: lp("/images/go_to_camera.svg")
            text: qsTr("Show on Camera")
            color: "transparent"
            onClicked: d.goToCamera()
        }

        Row
        {
            anchors.centerIn: parent

            IconButton
            {
                id: previousButton

                width: 48
                height: width
                icon.source: lp("images/previous_event.svg")
                icon.width: 24
                icon.height: icon.width
                padding: 0

                enabled: currentEventIndex < d.accessor.count - 1
                onClicked: ++currentEventIndex
            }

            IconButton
            {
                id: playPauseButton

                width: 48
                height: width
                icon.source: preview.playingPlayerState && !preview.cannotDecryptMedia
                    ? lp("images/pause.svg")
                    : lp("images/play_event.svg")

                icon.width: 24
                icon.height: icon.width
                padding: 0

                enabled: !preview.cannotDecryptMedia
                onClicked:
                {
                    if (preview.playingPlayerState)
                    {
                        preview.preview()
                        return
                    }

                    preview.play(slider.value == slider.to
                        ? preview.startTimeMs
                        : preview.position)
                }
            }

            IconButton
            {
                id: nextButton

                width: 48
                height: width
                icon.source: lp("images/next_event.svg")
                icon.width: 24
                icon.height: icon.width
                padding: 0

                enabled: currentEventIndex > 0
                onClicked: --currentEventIndex
            }
        }
    }

    Text
    {
        id: timestampText

        parent: LayoutController.isPortrait
            ? preview
            : playbackPanel

        visible: !preview.cannotDecryptMedia
        x: LayoutController.isPortrait
            ? 16
            : (downloadButton.visible ? downloadButton.x : orientationModeButton.x) - width - 16
        y: LayoutController.isPortrait
            ? parent.height - height - 14
            : (parent.height - height) / 2

        font.pixelSize: 16
        font.weight: Font.Medium
        color: ColorTheme.colors.light4

        text: EventSearchUtils.timestampText(slider.value, windowContext.mainSystemContext)
    }

    Rectangle
    {
        id: bottomNavigationArea

        y: parent.height
        x: -windowParams.leftMargin
        height: windowParams.bottomMargin
        width: parent.width + windowParams.leftMargin + windowParams.rightMargin
        color: ColorTheme.colors.dark1
    }

    Rectangle
    {
        id: navigationBarTint

        visible: mainWindow.hasNavigationBar

        x: windowParams.leftMargin ? -windowParams.leftMargin : parent.width
        y: -toolBar.height
        width: mainWindow.width - parent.width
        height: eventDetailsScreen.height

        color: ColorTheme.colors.dark3
    }

    Slider
    {
        id: slider

        property bool playerWasPlaying: false

        visible: opacity > 0 && !preview.cannotDecryptMedia
        opacity: d.hasControls ? 1 : 0


        x: playbackPanel.x
        y: playbackPanel.y - slider.height + (slider.height - sliderBackground.height) / 2
        width: playbackPanel.width
        height: 36
        leftPadding: 12
        rightPadding: 12

        from: preview.startTimeMs
        to: preview.startTimeMs + preview.durationMs

        handle: Rectangle
        {
            id: handle
            width: 24
            height: 24

            x: slider.visualPosition * (slider.width - slider.leftPadding - slider.rightPadding)
                + slider.leftPadding - handle.width / 2
            anchors.verticalCenter: slider.verticalCenter

            radius: 12
            color: ColorTheme.colors.light10
        }

        background: Rectangle
        {
            id: sliderBackground

            width: parent.width
            height: 4
            color: ColorTheme.colors.light17
            anchors.verticalCenter: slider.verticalCenter
        }

        onValueChanged:
        {
            if (!pressed && value == to)
                preview.preview()
        }

        onPressedChanged:
        {
            if (pressed)
            {
                playerWasPlaying = preview.playingPlayerState
                preview.preview()
            }
            else if (playerWasPlaying && value != to)
            {
                CoreUtils.executeLater(function()
                {
                    preview.play(preview.position)
                }, this)
            }
        }

        Connections
        {
            target: slider
            enabled: slider.pressed

            function onValueChanged()
            {
                preview.setPosition(slider.value)
            }
        }

        Binding
        {
            target: slider
            when: !slider.pressed
            property: "value"
            value: preview.atEnd ? slider.to : preview.position
        }
    }

    onEventSearchModelChanged: d.updateCurrentEvent()
    onCurrentEventIndexChanged: d.updateCurrentEvent()

    Binding
    {
        target: d
        property: "hasControls"
        value: LayoutController.isPortrait || d.showControls
    }

    QtObject
    {
        id: d

        property bool hasControls: true

        property ModelDataAccessor accessor: ModelDataAccessor {}
        property bool showControls: true
        property int exclusionAreaY: eventDetailsScreen.activePage
            ? slider.parent.mapToGlobal(0, slider.y).y * Screen.devicePixelRatio
            : 0

        function updateCurrentEvent()
        {
            if (!eventSearchModel || currentEventIndex < 0)
            {
                preview.resource = null
                preview.startTimeMs = 0
                preview.durationMs = 0
                preview.setPosition(0)
                return
            }

            preview.stop()

            const resource = accessor.getData(currentEventIndex, "resource")
            if (!resource)
                return;

            preview.resource = resource

            const paddingTimeMs = eventDetailsScreen.isAnalyticsDetails
                ? CoreSettings.iniConfigValue("previewPaddingTimeMs")
                : 0
            preview.startTimeMs =
                accessor.getData(currentEventIndex, "timestampMs") - paddingTimeMs
            preview.durationMs =
                accessor.getData(currentEventIndex, "durationMs") + paddingTimeMs * 2
            preview.setPosition(preview.startTimeMs)
            slider.value = preview.startTimeMs

            preview.play(preview.startTimeMs) //< Loads the stream anyway.
        }

        function updateStatusBarVisibility()
        {
            if (d.hasControls)
                windowContext.ui.windowHelpers.exitFullscreen()
            else
                windowContext.ui.windowHelpers.enterFullscreen()
        }

        function goToCamera()
        {
            Workflow.openVideoScreen(preview.resource, undefined, slider.from, camerasModel)
        }

        function updateGestureExclusionArea()
        {
            windowContext.ui.windowHelpers.setGestureExclusionArea(
                d.exclusionAreaY, slider.height * Screen.devicePixelRatio)
        }

        onHasControlsChanged: updateStatusBarVisibility()

        onExclusionAreaYChanged: updateGestureExclusionArea()
    }

    Connections
    {
        target: LayoutController

        function onIsPortraitChanged()
        {
            d.hasControls = true
        }
    }

    Component.onCompleted:
    {
        d.updateStatusBarVisibility()
        d.updateGestureExclusionArea()
    }
}
