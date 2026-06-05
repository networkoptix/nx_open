// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
import Nx.Items
import Nx.Mobile.Controls

import ".."

Item
{
    id: control

    property VideoScreenController controller
    property bool showPlaybackControls: true
    property bool hasChunkNavigation: true
    property alias cameraTitle: cameraTitleLabel.text
    property alias cameraTimestampText: cameraTimestampLabel.text

    property alias menuButtonControl: menuButton

    property alias actionsButtonVisible: actionsButton.visible
    property alias actionsButtonEnabled: actionsButton.enabled

    signal backButtonClicked()
    signal menuButtonClicked()
    signal actionsButtonClicked()
    signal exitFullscreenButtonClicked()

    property alias scrubbingActive: speedControl.pressed

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    function toggle()
    {
        opacityController.setOverlayVisible(!opacityController.overlayVisible)
    }

    onVisibleChanged:
    {
        if (visible)
            opacityController.setOverlayVisible(true)
    }

    QtObject
    {
        id: opacityController

        property bool overlayVisible: true

        // Opacity value expected for any overlay control or block unless otherwise specified.
        property real defaultControlsOpacity: overlayVisible && !control.scrubbingActive
            ? 1.0
            : 0.0

        Behavior on defaultControlsOpacity
        {
            NumberAnimation
            {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }

        // Opacity value for the speed controller.
        property real speedControlOpacity: control.scrubbingActive || overlayVisible
            ? 1.0
            : 0.0

        Behavior on speedControlOpacity
        {
            NumberAnimation
            {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }

        function setOverlayVisible(value)
        {
            overlayVisible = value
            reportUserActivity()
        }

        function reportUserActivity()
        {
            if (overlayVisible)
                userInactivityTimer.restart()
        }
    }

    Timer
    {
        id: userInactivityTimer
        interval: 5000
        repeat: false

        onTriggered: opacityController.setOverlayVisible(false)
    }

    // Rewind tap controls.
    FullscreenTapRewindControl
    {
        id: leftTapRewindControl

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        alignment: Qt.AlignLeft
        hintText: qsTr("-5 sec")

        visible: showPlaybackControls && !controller.playingLive
        opacity: opacityController.defaultControlsOpacity

        onActivated: controller.jumpDistance(-5000)
    }

    FullscreenTapRewindControl
    {
        id: rightTapRewindControl

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        alignment: Qt.AlignRight
        hintText: qsTr("+5 sec")

        visible: showPlaybackControls && !controller.playingLive
        opacity: opacityController.defaultControlsOpacity

        onActivated: controller.jumpDistance(5000)
    }

    Item
    {
        id: controlsContainer

        anchors.fill: parent

        visible: opacityController.defaultControlsOpacity > 0
        opacity: opacityController.defaultControlsOpacity

        // Top controls.
        GradientShadow
        {
            id: topControls

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            height: 120
            from: ColorTheme.transparent(ColorTheme.colors.dark3, 0.8)

            Item
            {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                height: 48

                OverlayControlButton
                {
                    id: backButton

                    anchors.top: parent.top
                    anchors.left: parent.left
                    icon.source: "image://skin/24x24/Outline/arrow_back.svg"

                    onClicked: control.backButtonClicked()
                }

                Column
                {
                    id: titleLabels

                    anchors.centerIn: parent

                    Text
                    {
                        id: cameraTitleLabel

                        anchors.horizontalCenter: parent.horizontalCenter
                        font { pixelSize: 18; weight: Font.Medium }
                        color: ColorTheme.colors.light4
                        lineHeight: 1.25
                    }
                    Text
                    {
                        id: cameraTimestampLabel

                        anchors.horizontalCenter: parent.horizontalCenter
                        font { pixelSize: 16; weight: Font.Normal }
                        color: ColorTheme.colors.light4
                    }
                }

                OverlayControlButton
                {
                    id: menuButton

                    anchors.top: parent.top
                    anchors.right: parent.right
                    icon.source: "image://skin/24x24/Outline/more.svg"

                    onClicked: control.menuButtonClicked()
                }
            }
        }

        // Left controls.
        Column
        {
            id: leftControls

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            OverlayControlButton
            {
                id: actionsButton

                icon.source: "image://skin/24x24/Outline/grid_view.svg"

                onClicked: control.actionsButtonClicked()
            }
        }

        // Center controls.
        Row
        {
            id: centerControls

            visible: control.showPlaybackControls

            anchors.centerIn: parent
            spacing: 44

            ControlButton
            {
                id: jumpToPreviousChunkButton

                y: 10
                radius: 100
                foregroundColor: ColorTheme.colors.light4
                backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
                icon.source: "image://skin/24x24/Outline/chunk_previous.svg"
                visible: control.hasChunkNavigation
                enabled: NxGlobals.isValidTime(controller.prevChunkMs)
                opacity: enabled ? 1 : 0.3

                onClicked: controller.jumpToPreviousChunk()
            }

            ControlButton
            {
                id: playPauseButton

                implicitWidth: 64;
                implicitHeight: 64;
                radius: 100
                foregroundColor: ColorTheme.colors.light4
                backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
                icon.source: controller.playing
                    ? "image://skin/24x24/Outline/pause.svg"
                    : "image://skin/24x24/Outline/play_small.svg"
                icon.width: 44
                icon.height: 44

                onClicked:
                {
                    if (controller.playing)
                        controller.pause()
                    else
                        controller.play()
                }
            }

            ControlButton
            {
                id: jumpToNextChunkButton

                y: 10
                radius: 100
                foregroundColor: ColorTheme.colors.light4
                backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
                icon.source: "image://skin/24x24/Outline/chunk_future.svg"
                visible: control.hasChunkNavigation
                enabled: !controller.playingLive
                opacity: enabled ? 1 : 0.3

                onClicked: controller.jumpToNextChunk()
            }
        }

        // Bottom controls.
        GradientShadow
        {
            id: bottomControls

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            height: 100
            from: "transparent"
            to: ColorTheme.transparent(ColorTheme.colors.dark3, 0.8)

            Item
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                height: 48

                LEDButton
                {
                    id: overlayLiveButton

                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    height: 48

                    leftPadding: 12
                    rightPadding: 12

                    backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)

                    text: "LIVE" //< Intentionally not translatable.
                    font.pixelSize: 14

                    checked: controller.playingLive

                    onClicked:
                    {
                        if (controller.playingLive)
                            controller.pause()
                        else
                            controller.playLive()
                    }
                }

                OverlayControlButton
                {
                    id: exitFullscreenButton

                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    icon.source: "image://skin/24x24/Outline/compress.svg"

                    onClicked: control.exitFullscreenButtonClicked()
                }
            }
        }
    }

    // Speed control
    SpeedControl
    {
        id: speedControl

        visible: control.showPlaybackControls && opacityController.speedControlOpacity > 0
        opacity: opacityController.speedControlOpacity

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 16

        height: 48

        color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
        radius: 6
        expanded: true
        expandedWidth: 400
        displayCollapseButton: false

        forced1x: controller.playingLive
        paused: !controller.playing

        onMoved:
        {
            if (speedControl.pausable)
            {
                if (speedControl.speed == 0)
                    controller.pause()
                else
                    controller.play()
            }

            controller.speed = speedControl.speed
        }

        // To block camera swipe if the speed control is dragged.
        DragHandler { target: null }
    }

    MouseArea
    {
        anchors.fill: parent
        hoverEnabled: true

        onPressed: (mouse) =>
        {
            opacityController.reportUserActivity()
            mouse.accepted = false
        }
    }

    Component.onCompleted:
    {
        userInactivityTimer.start()
    }

    // Components.
    component OverlayControlButton: ControlButton
    {
        width: 48
        height: 48
        foregroundColor: ColorTheme.colors.light4
        backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
    }
}
