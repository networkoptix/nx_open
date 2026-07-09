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

import nx.vms.client.core

Rectangle
{
    id: root

    property alias fullscreenLayout: d.fullscreenLayout
    property alias interval: preview

    property bool activePage: false
    property bool withNavigationControls: true
    property bool withShowOnCamera: true
    property alias hasPrevious: previousButton.enabled
    property alias hasNext: nextButton.enabled

    // State of the recorded data for the requested position:
    // - Available: data is present; the player and the playback controls are shown;
    // - Checking: archive presence is still being resolved; controls and placeholder are hidden;
    // - NoData: no archive at the position; the "No data" placeholder is shown and every control
    //   except "Show on camera" is disabled.
    enum DataState { Available, Checking, NoData }
    property int dataState: Preview.DataState.Available

    signal showFullscreen()
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

        return intervalHeight + (d.fullscreenLayout ? 0 : bottomPanel.height + 4)
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
        anchors.bottomMargin: d.fullscreenLayout
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

        onClicked: d.showControls = !d.showControls

        Image
        {
            y: parent.height - height + 1
            width: parent.width
            height: 56

            source: lp("/images/timeline_gradient.png")
            opacity: d.hasControls ? 1 : 0
            visible: !d.fullscreenLayout && opacity > 0
        }

        VideoDummy
        {
            anchors.fill: preview
            visible: preview.cannotDecryptMedia || root.dataState === Preview.DataState.NoData
            state: root.dataState === Preview.DataState.NoData ? "noData" : "cannotDecryptMedia"
        }

        RowLayout
        {
            id: centralFullscreenControls

            anchors.centerIn: parent
            visible: d.fullscreenLayout
            spacing: 44

            LayoutItemProxy { target: previousButton }

            LayoutItemProxy { target: playPauseButton }

            LayoutItemProxy { target: nextButton }
        }

        RowLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12

            visible: !d.fullscreenLayout

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: timestampText
            }

            LayoutItemProxy
            {
                target: fullscreenButton
                visible: root.dataState === Preview.DataState.Available
            }
        }
    }

    PreviewSlider
    {
        id: slider

        anchors.left: parent.left
        anchors.bottom: preview.bottom
        anchors.bottomMargin: - (height / 2) + 2
        anchors.right: parent.right

        preview: preview

        visible: opacity > 0 && !d.fullscreenLayout && !preview.cannotDecryptMedia
            && root.dataState === Preview.DataState.Available
        opacity: d.hasControls ? 1 : 0
    }

    Item
    {
        id: bottomPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: 68

        RowLayout
        {
            visible: !d.fullscreenLayout
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
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            visible: d.fullscreenLayout
            spacing: 8

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: timestampText
            }

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

    component ControlButton: Button
    {
        property bool rounded: false
        property bool transparent: rounded || d.fullscreenLayout

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

    ControlButton
    {
        id: fullscreenButton

        icon.source: d.fullscreenLayout
            ? "image://skin/24x24/Outline/exit_fullscreen_mode.svg"
            : "image://skin/24x24/Outline/fullscreen_view_mode.svg"

        implicitWidth: d.fullscreenLayout ? 44 : 24
        implicitHeight: d.fullscreenLayout ? 44 : 24

        opacity: !preview.cannotDecryptMedia ? 1.0 : 0.0
        enabled: opacity > 0 && root.dataState === Preview.DataState.Available
        backgroundColor: d.fullscreenLayout
            ? ColorTheme.transparent("#0D1012", enabled ? 0.5 : 0.15)
            : "transparent"

        onClicked: root.showFullscreen()
    }

    ControlButton
    {
        id: repeatButton

        icon.source: "image://skin/24x24/Solid/repeat.svg"

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

        opacity: d.hasControls
        enabled: root.hasPrevious && opacity > 0
        rounded: d.fullscreenLayout

        icon.source: "image://skin/24x24/Outline/chunk_previous.svg"
        icon.width: d.fullscreenLayout ? 32 : 24
        icon.height: d.fullscreenLayout ? 32 : 24

        onClicked: root.previous()
    }

    ControlButton
    {
        id: playPauseButton

        opacity: d.hasControls && !preview.cannotDecryptMedia ? 1.0 : 0.0
        enabled: opacity > 0 && root.dataState === Preview.DataState.Available
        rounded: d.fullscreenLayout

        implicitWidth: d.fullscreenLayout ? 64 : 44
        implicitHeight: d.fullscreenLayout ? 64 : 44

        icon.source: preview.playingPlayerState && !preview.cannotDecryptMedia
            ? "image://skin/24x24/Outline/pause.svg"
            : "image://skin/24x24/Outline/play_small.svg"
        icon.width: d.fullscreenLayout ? 64 : 24
        icon.height: d.fullscreenLayout ? 64 : 24

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

        opacity: d.hasControls
        enabled: root.hasNext && opacity > 0
        rounded: d.fullscreenLayout

        icon.source: "image://skin/24x24/Outline/chunk_future.svg"
        icon.width: d.fullscreenLayout ? 32 : 24
        icon.height: d.fullscreenLayout ? 32 : 24

        onClicked: root.next()
    }

    ControlButton
    {
        id: showOnCameraButton

        opacity: (d.hasControls && root.withShowOnCamera) ? 1 : 0
        enabled: opacity > 0
        icon.source: "image://skin/24x24/Outline/show_on_layout.svg"

        onClicked: root.showOnCamera()
    }

    Binding
    {
        target: d
        property: "hasControls"
        value: !d.fullscreenLayout || d.showControls
    }

    QtObject
    {
        id: d

        function updateStatusBarVisibility()
        {
            if (d.hasControls)
                windowContext.ui.windowHelpers.exitFullscreen()
            else
                windowContext.ui.windowHelpers.enterFullscreen()
        }

        property bool fullscreenLayout: false
        onFullscreenLayoutChanged: d.hasControls = true

        property bool hasControls: true
        onHasControlsChanged: updateStatusBarVisibility()

        property bool showControls: true

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
}
