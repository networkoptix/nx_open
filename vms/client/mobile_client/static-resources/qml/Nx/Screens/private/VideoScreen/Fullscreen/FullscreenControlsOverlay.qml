// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Items
import Nx.Mobile.Controls
import Nx.Ui

import nx.vms.client.core

import ".."

Item
{
    id: control

    property VideoScreenController controller
    property bool showPlaybackControls: true
    property bool hasChunkNavigation: true

    // Whether the action button container should take place in the layouts. Cannot be replaced
    // with the `actionButtonContainer.visible` binding, since the container visibility is
    // controlled by LayoutItemProxy.
    property bool hasActionButton: false

    property alias cameraTitle: cameraTitleLabel.text
    property alias cameraTimestampText: cameraTimestampLabel.text

    property alias menuButtonControl: menuButton

    property alias actionButtonContainer: overlayActionButtonContainer

    signal backButtonClicked()
    signal menuButtonClicked()
    signal exitFullscreenButtonClicked()

    property alias scrubbingActive: speedControl.pressed

    readonly property int tapRewindDistanceMs: 5000

    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    function toggle()
    {
        opacityController.setOverlayVisible(!opacityController.overlayVisible)
    }

    function showDurationHint(durationMs)
    {
        opacityController.setOverlayVisible(true)

        const sign = durationMs > 0 ? "+" : "-"
        const durationString = Duration.toString(Math.abs(durationMs),
            Duration.DaysAndTime, Duration.Long, " ")

        hintBanner.showBanner("%1 %2"
            .arg(sign)
            .arg(durationString))
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
            if (speedControl.menuOpened)
                userInactivityTimer.stop()
            else if (overlayVisible)
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
        hintText: "-%1".arg(Duration.toString(tapRewindDistanceMs, Duration.Seconds, Duration.Long))

        visible: showPlaybackControls

        onTapped: control.toggle()
        onActivated:
        {
            const skippedMs = controller.jumpDistance(-tapRewindDistanceMs)
            if (Math.abs(skippedMs) > tapRewindDistanceMs)
                showDurationHint(skippedMs)
        }
    }

    FullscreenTapRewindControl
    {
        id: rightTapRewindControl

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: parent.top
        alignment: Qt.AlignRight
        hintText: controller.playingLive
            ? qsTr("You are in Live Mode")
            : "+%1".arg(Duration.toString(tapRewindDistanceMs, Duration.Seconds, Duration.Long))
        rewindAnimationEnabled: !controller.playingLive

        visible: showPlaybackControls

        onTapped: control.toggle()
        onActivated:
        {
            if (controller.playingLive)
                return

            const skippedMs = controller.jumpDistance(tapRewindDistanceMs)
            if (Math.abs(skippedMs) > tapRewindDistanceMs)
                showDurationHint(skippedMs)
        }
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
                    spacing: 5

                    Row
                    {
                        id: titleRow

                        anchors.horizontalCenter: parent.horizontalCenter
                        height: cameraTitleLabel.height

                        RecordingStatusIndicator
                        {
                            id: recordingStatusIndicator

                            anchors.verticalCenter: titleRow.verticalCenter
                            resource: controller.resource
                        }

                        Text
                        {
                            id: cameraTitleLabel

                            font { pixelSize: 18; weight: Font.Medium }
                            color: ColorTheme.colors.light4
                        }
                    }

                    Text
                    {
                        id: cameraTimestampLabel

                        anchors.horizontalCenter: parent.horizontalCenter
                        font { pixelSize: 16; weight: Font.Normal }
                        color: ColorTheme.colors.light4
                    }
                }

                Rectangle
                {
                    id: hintBanner

                    anchors.horizontalCenter: titleLabels.horizontalCenter
                    anchors.top: titleLabels.bottom
                    anchors.topMargin: 8

                    width: hintText.implicitWidth + 20
                    height: hintText.implicitHeight + 12

                    radius: 4
                    color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)

                    opacity: 0
                    visible: opacity > 0

                    Behavior on opacity
                    {
                        NumberAnimation
                        {
                            duration: 200
                            easing.type: Easing.InOutQuad
                        }
                    }

                    Text
                    {
                        id: hintText

                        anchors.centerIn: parent

                        font { pixelSize: 16; weight: Font.Normal }
                        color: ColorTheme.colors.light4
                    }

                    Timer
                    {
                        id: hideTimer

                        interval: 2000
                        repeat: false
                        onTriggered: hintBanner.opacity = 0
                    }

                    function showBanner(text)
                    {
                        hintText.text = text
                        opacity = 1
                        hideTimer.restart()
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
    }

    Item
    {
        anchors.fill: parent
        visible: opacityController.defaultControlsOpacity > 0
            || opacityController.speedControlOpacity > 0

        // Left controls.
        Column
        {
            id: leftControls

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: !LayoutController.isPortrait

            LayoutItemProxy
            {
                target: overlayActionButtonContainer
                visible: control.hasActionButton
            }
        }

        GradientShadow
        {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            height: LayoutController.isPortrait ? 164 : 100
            from: "transparent"
            to: ColorTheme.transparent(ColorTheme.colors.dark3, 0.8)
            opacity: opacityController.defaultControlsOpacity
        }

        // Bottom controls landscape layout.
        RowLayout
        {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16

            visible: !LayoutController.isPortrait

            LayoutItemProxy
            {
                target: overlayLiveButton
            }

            Item
            {
                Layout.fillWidth: true
            }

            LayoutItemProxy
            {
                Layout.preferredWidth: speedControl.implicitWidth

                target: speedControl
                visible: control.showPlaybackControls && opacityController.speedControlOpacity > 0
            }

            Item
            {
                Layout.fillWidth: true
            }

            LayoutItemProxy
            {
                target: exitFullscreenButton
            }
        }

        // Bottom controls portrait layout.
        ColumnLayout
        {
            id: portraitBottomControlsLayout

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16

            visible: LayoutController.isPortrait

            spacing: 16

            LayoutItemProxy
            {
                Layout.fillWidth: true

                target: speedControl
                visible: control.showPlaybackControls && opacityController.speedControlOpacity > 0
            }

            RowLayout
            {
                Layout.fillWidth: true

                spacing: 8

                LayoutItemProxy
                {
                    target: overlayLiveButton
                }

                LayoutItemProxy
                {
                    target: overlayActionButtonContainer
                    visible: control.hasActionButton
                }

                Item
                {
                    Layout.fillWidth: true
                }

                LayoutItemProxy
                {
                    target: exitFullscreenButton
                }
            }
        }
    }

    Item
    {
        id: overlayActionButtonContainer

        implicitWidth: 48
        implicitHeight: 48

        opacity: opacityController.defaultControlsOpacity
        enabled: opacity > 0
    }

    LEDButton
    {
        id: overlayLiveButton

        implicitHeight: 48

        opacity: opacityController.defaultControlsOpacity
        enabled: opacity > 0

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

        icon.source: "image://skin/24x24/Outline/compress.svg"

        opacity: opacityController.defaultControlsOpacity
        enabled: opacity > 0

        onClicked: control.exitFullscreenButtonClicked()
    }

    SpeedControl
    {
        id: speedControl

        opacity: opacityController.speedControlOpacity
        color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
        margins: 2
        radius: 6
        expanded: true
        expandedWidth: LayoutController.isPortrait ? portraitBottomControlsLayout.width : 400
        displayCollapseButton: false

        forced1x: controller.playingLive
        paused: !controller.playing

        onMoved:
            controller.setSpeed(speed)

        onMenuOpenedChanged:
            opacityController.reportUserActivity()

        // Reflect the actual playback speed so the control stays correct when the speed is
        // changed elsewhere (e.g. the normal-mode controls).
        Binding
        {
            target: speedControl
            property: "speed"
            value: controller.speed
            when: !speedControl.pressed
            restoreMode: Binding.RestoreNone
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
        implicitWidth: 48
        implicitHeight: 48
        foregroundColor: ColorTheme.colors.light4
        backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
    }
}
