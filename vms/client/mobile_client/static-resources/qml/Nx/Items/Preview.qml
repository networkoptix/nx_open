// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick
import QtQuick.Layouts

import Nx.Mobile
import Nx.Controls
import Nx.Core
import Nx.Core.Items
import Nx.Core.Ui
import Nx.Mobile.Controls
import Nx.Screens
import Nx.Ui

import nx.vms.client.core

Rectangle
{
    id: root

    required property string title //< Required for fullscreen title.

    property alias interval: preview

    property bool activePage: false
    property bool withNavigationControls: true
    property bool withShowOnCamera: true

    property bool hasPrevious: true
    property bool hasNext: true

    // State of the recorded data for the requested position:
    // - Available: data is present; the player and the playback controls are shown;
    // - Checking: archive presence is still being resolved; controls and placeholder are hidden;
    // - NoData: no archive at the position; the "No data" placeholder is shown and every control
    //   except "Show on camera" is disabled.
    enum DataState { Available, Checking, NoData }
    property int dataState: Preview.DataState.Available

    signal back()
    signal previous()
    signal next()
    signal showOnCamera()

    function heightForWidth(h)
    {
        let intervalHeight = width / 16 * 9

        if (interval.isReady)
        {
            intervalHeight = interval.videoRotation === 0 || interval.videoRotation === 180
                ? width / interval.videoAspectRatio
                : width * interval.videoAspectRatio
        }

        return intervalHeight + (LayoutController.fullscreen ? 0 : bottomPanel.height + 4)
    }

    color: ColorTheme.colors.dark3

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

        anchors.fill: parent
        anchors.bottomMargin: LayoutController.fullscreen
            ? 0
            : bottomPanel.height

        audioEnabled: audioController.audioEnabled
        scalable: true
        hasPreloader: root.dataState !== Preview.DataState.NoData
        preloaderColor: ColorTheme.colors.light10
        preloaderDotRadius: 6
        aspectRatio: 0 //< No forced aspect ratio.
        active: true
        autoplay: false
        autoRepeat: false
        color: "transparent"

        onClicked:
        {
            if (LayoutController.fullscreen)
                d.setControlsVisible(!d.controlsVisible)
        }

        VideoDummy
        {
            anchors.fill: preview
            visible: preview.cannotDecryptMedia || root.dataState === Preview.DataState.NoData
            state: root.dataState === Preview.DataState.NoData ? "noData" : "cannotDecryptMedia"
        }

        GradientShadow
        {
            height: 120
            width: parent.width
            from: ColorTheme.transparent("#0D1012", 0.8)
            opacity: d.controlsOpacity
            visible: LayoutController.fullscreen
        }

        GradientShadow
        {
            anchors.bottom: parent.bottom
            height: 120
            width: parent.width
            from: ColorTheme.transparent("#0D1012", 0.8)
            rotation: 180
            opacity: d.controlsOpacity
            visible: LayoutController.fullscreen
        }

        ControlButton
        {
            id: backButton

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 16

            opacity: d.controlsOpacity
            visible: opacity > 0 && LayoutController.fullscreen

            icon.source: "image://skin/24x24/Outline/arrow_back.svg"

            onClicked: root.back()
        }

        ColumnLayout
        {
            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter

            spacing: 4
            opacity: d.controlsOpacity
            visible: LayoutController.fullscreen && opacity > 0

            Text
            {
                Layout.alignment: Qt.AlignHCenter

                text: root.title

                font.pixelSize: 18
                font.weight: Font.Medium

                color: ColorTheme.colors.light4
            }

            LayoutItemProxy
            {
                target: timestampText
            }
        }

        RowLayout
        {
            id: centralFullscreenControls

            anchors.centerIn: parent
            visible: LayoutController.fullscreen
            spacing: 44

            LayoutItemProxy
            {
                target: previousButton
                visible: root.withNavigationControls
            }

            LayoutItemProxy { target: playPauseButton }

            LayoutItemProxy
            {
                target: nextButton
                visible: root.withNavigationControls
            }
        }

        LayoutItemProxy
        {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12

            target: fullscreenButton
            visible: !LayoutController.fullscreen && root.dataState === Preview.DataState.Available
        }

        LayoutItemProxy
        {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 12

            target: timestampText
            visible: !LayoutController.fullscreen
        }
    }

    PreviewSlider
    {
        id: slider

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: preview.bottom
        anchors.bottomMargin: LayoutController.fullscreen ? 8 : - (height / 2) + 2

        preview: preview

        visible: opacity > 0 && root.dataState === Preview.DataState.Available && !preview.cannotDecryptMedia
        opacity: d.controlsOpacity
    }

    Item
    {
        id: bottomPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: LayoutController.fullscreen ? 36 : 0

        height: 68

        RowLayout
        {
            visible: !LayoutController.fullscreen
            spacing: 8

            anchors.fill: parent
            anchors.margins: 12

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: repeatButton
            }

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: previousButton
                visible: root.withNavigationControls
            }

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: playPauseButton
            }

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: nextButton
                visible: root.withNavigationControls
            }

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: showOnCameraButton
            }
        }

        RowLayout
        {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: 16

            visible: LayoutController.fullscreen && d.controlsOpacity > 0
            spacing: 8

            LayoutItemProxy
            {
                target: repeatButton
            }

            LayoutItemProxy
            {
                target: showOnCameraButton
            }

            LayoutItemProxy
            {
                target: fullscreenButton
            }
        }
    }

    Text
    {
        id: timestampText

        font.pixelSize: 16
        font.weight: Font.Medium

        color: ColorTheme.colors.light4
        opacity: !preview.cannotDecryptMedia && root.dataState === Preview.DataState.Available
            ? 1.0
            : 0.0

        text: EventSearchUtils.timestampText(
            slider.value, windowContext.mainSystemContext, /*alwaysShowDate*/ true)
    }

    ControlButton
    {
        id: fullscreenButton

        icon.source: LayoutController.fullscreen
            ? "image://skin/24x24/Outline/exit_fullscreen_mode.svg"
            : "image://skin/24x24/Outline/fullscreen_view_mode.svg"

        implicitWidth: LayoutController.fullscreen ? 44 : 24
        implicitHeight: LayoutController.fullscreen ? 44 : 24

        opacity: preview.cannotDecryptMedia
            ? 0.0
            : (LayoutController.fullscreen ? d.controlsOpacity : 1.0)

        enabled: opacity > 0 && root.dataState === Preview.DataState.Available
            && (LayoutController.fullscreen || preview.isReady)

        backgroundColor: LayoutController.fullscreen
            ? ColorTheme.transparent("#0D1012", enabled ? 0.5 : 0.15)
            : "transparent"

        onClicked: LayoutController.toggleFullscreen(
            d.verticalVideo ? Qt.PortraitOrientation : Qt.LandscapeOrientation)
    }

    ControlButton
    {
        id: repeatButton

        icon.source: "image://skin/24x24/Solid/repeat.svg"

        opacity: d.controlsOpacity
        enabled: root.dataState === Preview.DataState.Available
        checkable: true
        checked: preview.autoRepeat

        onClicked:
        {
            preview.autoRepeat = checked
            preview.autoplay = checked
        }
    }

    ControlButton
    {
        id: previousButton

        opacity: d.controlsOpacity
        enabled: root.hasPrevious && opacity > 0
        rounded: LayoutController.fullscreen

        icon.source: "image://skin/24x24/Outline/chunk_previous.svg"
        icon.width: LayoutController.fullscreen ? 32 : 24
        icon.height: LayoutController.fullscreen ? 32 : 24

        onClicked: root.previous()
    }

    ControlButton
    {
        id: playPauseButton

        opacity: preview.cannotDecryptMedia ? 0.0 : d.controlsOpacity
        enabled: opacity > 0 && root.dataState === Preview.DataState.Available
        rounded: LayoutController.fullscreen

        implicitWidth: LayoutController.fullscreen ? 64 : 44
        implicitHeight: LayoutController.fullscreen ? 64 : 44

        icon.source: preview.playingPlayerState && !preview.cannotDecryptMedia
            ? "image://skin/24x24/Outline/pause.svg"
            : "image://skin/24x24/Outline/play_small.svg"
        icon.width: LayoutController.fullscreen ? 64 : 24
        icon.height: LayoutController.fullscreen ? 64 : 24

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

    ControlButton
    {
        id: nextButton

        opacity: d.controlsOpacity
        enabled: root.hasNext && opacity > 0
        rounded: LayoutController.fullscreen

        icon.source: "image://skin/24x24/Outline/chunk_future.svg"
        icon.width: LayoutController.fullscreen ? 32 : 24
        icon.height: LayoutController.fullscreen ? 32 : 24

        onClicked: root.next()
    }

    ControlButton
    {
        id: showOnCameraButton

        opacity: root.withShowOnCamera ? d.controlsOpacity : 0.0
        enabled: opacity > 0
        icon.source: "image://skin/24x24/Outline/show_on_layout.svg"

        onClicked: root.showOnCamera()
    }

    Connections
    {
        target: LayoutController

        // Each fullscreen session starts with the controls visible.
        function onFullscreenChanged()
        {
            d.setControlsVisible(true)
        }
    }

    Timer
    {
        id: userInactivityTimer

        interval: 5000
        repeat: false

        onTriggered: d.setControlsVisible(false)
    }

    MouseArea
    {
        anchors.fill: parent

        onPressed: (mouse) =>
        {
            d.reportUserActivity()
            mouse.accepted = false
        }
    }

    QtObject
    {
        id: d

        property bool controlsVisible: true

        function setControlsVisible(value)
        {
            controlsVisible = value
            reportUserActivity()
        }

        function reportUserActivity()
        {
            if (LayoutController.fullscreen && controlsVisible)
                userInactivityTimer.restart()
            else
                userInactivityTimer.stop()
        }

        property real controlsOpacity: !LayoutController.fullscreen || controlsVisible
            ? 1.0
            : 0.0

        Behavior on controlsOpacity
        {
            NumberAnimation
            {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }

        readonly property bool verticalVideo: preview.isReady
            && (preview.videoRotation === 0 || preview.videoRotation === 180
                ? preview.videoAspectRatio < 1
                : preview.videoAspectRatio > 1)

        property int exclusionAreaY: root.activePage
            ? slider.parent.mapToGlobal(0, slider.y).y * Screen.devicePixelRatio
            : 0

        function updateGestureExclusionArea()
        {
            windowContext.ui.windowHelpers.setGestureExclusionArea(
                d.exclusionAreaY, slider.height * Screen.devicePixelRatio)
        }

        onExclusionAreaYChanged: updateGestureExclusionArea()
    }

    Component.onCompleted:
    {
        d.updateGestureExclusionArea()
    }

    component ControlButton: Button
    {
        property bool rounded: false
        property bool transparent: rounded || LayoutController.fullscreen

        implicitWidth: 44
        implicitHeight: 44

        type: Button.Type.Interface
        radius: rounded ? width / 2 : 6

        icon.width: 24
        icon.height: 24

        backgroundColor: transparent
            ? ColorTheme.transparent("#0D1012", enabled ? 0.5 : 0.15)
            : parameters.colors[state]

        foregroundColor: transparent
            ? ColorTheme.transparent(ColorTheme.colors.light1, enabled ? 1.0 : 0.3)
            : parameters.textColors[state]
    }
}
