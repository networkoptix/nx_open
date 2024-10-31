// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx.Core
import Nx.Controls
import Nx.Core.Controls
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Items
import Nx.Mobile
import Nx.Models
import Nx.Ui

import nx.vms.client.core

import "items"

PageBase
{
    id: eventDetailsScreen

    objectName: "eventDetailsScreen"

    property bool isAnalyticsDetails: true
    property QnCameraListModel camerasModel: null
    property alias eventSearchModel: d.accessor.model
    property int currentEventIndex: -1

    sideNavigationEnabled: false
    header: ToolBar
    {
        id: toolBar

        visible: opacity > 0
        opacity: d.hasControls ? 1 : 0

        contentItem.y: deviceStatusBarHeight

        title: qsTr("Preview")
        contentItem.clip: false
        leftButtonIcon.source: lp("/images/arrow_back.png")
        onLeftButtonClicked: Workflow.popCurrentScreen()

        background: Image
        {
            y: -toolBar.statusBarHeight
            x: -mainWindow.leftPadding
            width: mainWindow.width
            height: parent.height
            source: lp("/images/toolbar_gradient.png")
            opacity: d.isPortraitLayout ? 0 : 1
        }

        controls:
        [
            DownloadMediaButton
            {
                id: downloadButton

                enabled: preview.player && !preview.player.cannotDecryptMediaError
                resourceId: preview.resource && preview.resource.id
                positionMs: preview.startTimeMs
                durationMs: preview.durationMs
            }
        ]
    }

    Rectangle
    {
        color: ColorTheme.colors.dark3
        anchors.fill: preview
    }

    AudioController
    {
        id: audioController

        resource: preview.resource
        serverSessionManager: sessionManager
    }

    MediaPlaybackInterruptor
    {
        player: preview.player
        interruptOnInactivity: CoreUtils.isMobile()
    }

    IntervalPreview
    {
        id: preview

        audioEnabled: audioController.audioEnabled && eventDetailsScreen.activePage

        y: d.isPortraitLayout
           ? deviceStatusBarHeight
           : header.visible ? -header.height : 0
        x: -mainWindow.leftPadding

        width:
        {
            return d.isPortraitLayout
                ? parent.width
                : mainWindow.width
        }

        height:
        {
            if (!d.isPortraitLayout)
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
            visible: d.isPortraitLayout && opacity > 0
        }

        IconButton
        {
            id: goFullscreenModeButton

            x: parent.width - width
            y: parent.height - height
            width: 48
            height: 48
            visible: d.isPortraitLayout

            icon.source: lp("images/fullscreen_view_mode.svg")
            icon.width: 24
            icon.height: icon.width
            padding: 0

            onClicked:
            {
                if (CoreUtils.isMobile())
                    setScreenOrientation(Qt.LandscapeOrientation)
                else
                    [mainWindow.width, mainWindow.height] = [mainWindow.height, mainWindow.width]
            }
        }
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

            TagsView
            {
                width: parent.width
                model: d.accessor.getData(currentEventIndex, "tags")
                visible: hasTags
            }

            Text
            {
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
                width: parent.width

                color: ColorTheme.colors.light10
                font.pixelSize: 14
                font.weight: Font.Normal
                wrapMode: Text.Wrap
                text: d.accessor.getData(currentEventIndex, "description")
                visible: !!text
            }

            NameValueTable
            {
                width: parent.width

                nameFont.pixelSize: 16
                nameColor: ColorTheme.colors.light16
                valueFont.pixelSize: 16
                valueColor: ColorTheme.colors.light4
                valueFont.weight: Font.Medium
                items: d.accessor.getData(currentEventIndex, "attributes") ?? []
            }
        }
    }

    Rectangle
    {
        id: playbackPanel

        readonly property real navigationHeight: mainWindow.height - eventDetailsScreen.height
        visible: opacity > 0
        opacity:
        {
            if (!d.hasControls)
                return 0
            return d.portrait ? 1 : 0.8
        }

        x: mainWindow.hasNavigationBar ? 0 : -mainWindow.leftPadding
        y: parent.height - height + navigationHeight
        width: mainWindow.hasNavigationBar ? parent.width : mainWindow.width
        height: 76 + navigationHeight

        color: ColorTheme.colors.dark1

        Button
        {
            labelPadding: 12
            anchors.verticalCenter: parent.verticalCenter
            visible: !d.isPortraitLayout

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
                width: 48
                height: width
                icon.source: preview.playingPlayerState
                    ? lp("images/pause.svg")
                    : lp("images/play_event.svg")

                icon.width: 24
                icon.height: icon.width
                padding: 0

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
        parent: d.isPortraitLayout
            ? preview
            : playbackPanel

        x: d.isPortraitLayout
            ? 16
            : parent.width - width - 16 - (headerButton.visible ? headerButton.width + 16 : 0)
        y: d.isPortraitLayout
            ? parent.height - height - 14
            : (parent.height - height) / 2

        font.pixelSize: 16
        font.weight: Font.Medium
        color: ColorTheme.colors.light4

        text: EventSearchUtils.timestampText(slider.value)
    }

    IconButton
    {
        id: headerButton

        parent: d.isPortraitLayout
            ? preview
            : playbackPanel

        x: d.isPortraitLayout
            ? parent.width - width - goFullscreenModeButton.width
            : parent.width - width - 16;

        y: d.isPortraitLayout
            ? parent.height - height
            : (parent.height - height) / 2
        padding: 0
        icon.source: d.isPortraitLayout
            ? lp("/images/go_to_camera.svg")
            : lp("/images/exit_fullscreen_mode.svg")
        icon.width: 24
        icon.height: icon.height
        onClicked:
        {
            if (d.isPortraitLayout)
            {
                d.goToCamera()
            }
            else
            {
                // Go to portrait mode.
                if (CoreUtils.isMobile())
                    setScreenOrientation(Qt.PortraitOrientation)
                else
                    [mainWindow.width, mainWindow.height] = [mainWindow.height, mainWindow.width]
            }
        }
    }

    Rectangle
    {
        id: navigationBarTint

        visible: mainWindow.hasNavigationBar

        x: mainWindow.leftPadding ? -mainWindow.leftPadding : parent.width
        y: -toolBar.height
        width: mainWindow.width - parent.width
        height: eventDetailsScreen.height

        color: ColorTheme.colors.dark3
    }

    Slider
    {
        id: slider

        property bool playerWasPlaying: false

        visible: opacity > 0
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
        value: d.isPortraitLayout || d.showControls
    }

    QtObject
    {
        id: d

        property bool hasControls: true

        property ModelDataAccessor accessor: ModelDataAccessor {}
        property bool showControls: true
        property bool isPortraitLayout: mainWindow.width < mainWindow.height
        property int exclusionAreaY: eventDetailsScreen.activePage
            ? slider.parent.mapToGlobal(0, slider.y).y * Screen.devicePixelRatio
            : 0

        function updateCurrentEvent()
        {
            if (!eventSearchModel || currentEventIndex < 0)
            {
                preview.resource = undefined
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
            preview.startTimeMs = accessor.getData(currentEventIndex, "timestampMs")
            preview.durationMs = d.getPreviewDurationMs(currentEventIndex)
            preview.setPosition(preview.startTimeMs)
            slider.value = preview.startTimeMs

            preview.play(preview.startTimeMs) //< Loads the stream anyway.
        }

        function getPreviewDurationMs(index)
        {
            // Duration for objects can't be less than the specified value in ini configuration.
            const kMinimalDurationMs = CoreSettings.iniConfigValue("intervalPreviewDurationMs")
            const durationMs = accessor.getData(index, "durationMs")
            return eventDetailsScreen.isAnalyticsDetails
                ? Math.max(durationMs, kMinimalDurationMs)
                : durationMs
        }

        function updateStatusBarVisibility()
        {
            if (d.hasControls)
                exitFullscreen()
            else
                enterFullscreen()
        }

        function goToCamera()
        {
            Workflow.openVideoScreen(
                preview.resource, undefined, 0, 0, slider.from, camerasModel)
        }

        function updateGestureExclusionArea()
        {
            setGestureExclusionArea(d.exclusionAreaY, slider.height * Screen.devicePixelRatio)
        }

        onHasControlsChanged: updateStatusBarVisibility()

        onIsPortraitLayoutChanged: d.hasControls = true

        onExclusionAreaYChanged: updateGestureExclusionArea()
    }

    Component.onCompleted:
    {
        d.updateStatusBarVisibility()
        d.updateGestureExclusionArea()
    }
}
