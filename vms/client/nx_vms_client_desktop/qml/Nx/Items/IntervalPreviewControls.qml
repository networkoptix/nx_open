// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Window 2.15

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

Item
{
    id: controls

    property var preview: null
    property var selectedItem: null

    property bool nextEnabled: false
    property bool prevEnabled: false

    readonly property int iconSize: 20

    height: 4 + 12 + 8 + 32 + 8

    signal prevClicked()
    signal nextClicked()

    ProgressBar
    {
        id: progress

        barHeight: 3

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 4 + 4

        from: preview ? preview.startTimeMs : 0
        to: preview ? (preview.startTimeMs + preview.durationMs) : 0
        value: positionWatcher.positionMs

        showText: false

        QtObject
        {
            // Provides smooth animation of slider when preview position changes are reported
            // too far apart. Watches the difference between current and previous position and
            // setups slider animation time accordingly.

            id: positionWatcher

            property int animationDurationMs: 0
            property var previousPositionMs: 0

            readonly property var positionMs: selectedItem && preview && preview.isReady
                ? preview.position
                : progress.from

            function onPositionMsChanged()
            {
                const positionDiffMs = positionMs - previousPositionMs
                previousPositionMs = positionMs

                // Use slider animation only during regular playback.
                animationDurationMs = preview && preview.isReady
                    ? Math.max(positionDiffMs / preview.speedFactor, 0)
                    : 0
            }
        }

        Behavior on value
        {
            enabled: preview && preview.isReady

            NumberAnimation
            {
                easing.type: Easing.Linear
                // Avoid running the animation when user drags or presses the slider.
                duration: sliderArea.pressed ? 0 : positionWatcher.animationDurationMs
            }
        }

        enabled: !!selectedItem && preview.isReady
    }

    MouseArea
    {
        id: sliderArea

        anchors.verticalCenter: progress.verticalCenter
        anchors.verticalCenterOffset: -1
        height: 12 + 2
        anchors.left: progress.left
        anchors.right: progress.right

        property bool resume: false
        hoverEnabled: true

        enabled: progress.enabled

        onPressed: (mouse) =>
        {
            if (preview.playing)
                resume = !preview.atEnd

            preview.pause()
            setPosition(mouse.x)
        }

        onReleased:
        {
            if (resume)
                preview.play(preview.position)
            resume = false
        }

        onPositionChanged: (mouse) =>
        {
            if (pressed)
                setPosition(mouse.x)
        }

        function setPosition(x)
        {
            const clampedX = Math.min(x, width - 1)
            const normalized = Math.max(0.0, Math.min(1.0, clampedX / width))
            const timestampMs = preview.startTimeMs + preview.durationMs * normalized
            preview.setPosition(timestampMs)
        }
    }

    Rectangle
    {
        id: slider

        width: 6
        height: 12
        radius: 1
        color:
        {
            if (sliderArea.containsPress)
                return ColorTheme.colors.light10

            if (sliderArea.containsMouse)
                return ColorTheme.colors.light1

            return ColorTheme.colors.light4
        }

        x: progress.x + progress.position * progress.width - width / 2
        anchors.verticalCenter: progress.verticalCenter
        anchors.verticalCenterOffset: -0.5
        visible: progress.enabled
    }

    Item
    {
        id: buttons

        property bool enabled: preview && selectedItem

        anchors.top: controls.top
        anchors.topMargin: 4 + 12 + 8
        anchors.bottom: parent.bottom

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8

        component ControlButton: ImageButton
        {
            id: button

            property string tooltipText: ""

            width: 6 + icon.width + 6
            height: 32
            radius: 2

            icon.color: "transparent"

            normalBackground: ColorTheme.colors.dark8
            hoveredBackground: ColorTheme.colors.dark9
            pressedBackground: ColorTheme.colors.dark5
            opacity: enabled ? 1.0 : 0.3

            ToolTip
            {
                parent: button

                // Place below parent on the right side.
                x: parent.width / 2
                y: parent.height + 12

                visible: button.hovered
                text: button.tooltipText
                topPadding: 5
                bottomPadding: 5
            }
        }

        ControlButton
        {
            id: muteButton

            enabled: buttons.enabled
                && AudioDispatcher.audioEnabled
                && preview.audioSupported
                && preview.isReady

            anchors.left: parent.left

            property bool muted: false

            icon.width: 24
            icon.source:
            {
                const muted = muteButton.muted || !AudioDispatcher.audioEnabled
                const filename = muted ? "unmute.svg" : "mute.svg"
                return `image://svg/skin/advanced_search_dialog/${filename}`
            }

            tooltipText: qsTr("Toggle Mute")

            onClicked:
            {
                muted = !muted

                if (muted)
                    AudioDispatcher.releaseAudio(controls.Window.window)
                else
                    AudioDispatcher.requestAudio(controls.Window.window)
            }

            Component.onCompleted:
            {
                if (!muted)
                    AudioDispatcher.requestAudio(controls.Window.window)
            }

            Binding
            {
                target: controls.preview
                when: !!controls.preview
                property: "audioEnabled"
                value: AudioDispatcher.currentAudioSource === controls.Window.window
            }
        }

        Row
        {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 1

            ControlButton
            {
                id: prevButton

                enabled: controls.prevEnabled && buttons.enabled

                icon.source: "image://svg/skin/advanced_search_dialog/previous.svg"
                tooltipText: qsTr("Previous Object")

                onClicked:
                    controls.prevClicked()
            }

            ControlButton
            {
                id: playPauseButton

                enabled: buttons.enabled && preview.isReady

                icon.source: `image://svg/skin/advanced_search_dialog/${preview.playing ? "pause" : "play"}.svg`
                tooltipText: preview.playing ? qsTr("Pause") : qsTr("Play")

                onClicked:
                {
                    if (preview.playing)
                    {
                        preview.pause()
                        return
                    }

                    preview.play(preview.atEnd
                        ? preview.startTimeMs
                        : preview.position)
                }
            }

            ControlButton
            {
                id: nextButton

                enabled: controls.nextEnabled && buttons.enabled

                icon.source: "image://svg/skin/advanced_search_dialog/next.svg"
                tooltipText: qsTr("Next Object")

                onClicked:
                    controls.nextClicked()
            }
        }

        ControlButton
        {
            id: autoRepeatButton

            enabled: buttons.enabled && preview.isReady
            anchors.right: parent.right
            icon.source: "image://svg/skin/advanced_search_dialog/repeat.svg"
            icon.width: 24

            tooltipText: qsTr("Toggle Repeat")

            checkable: true
            checked: preview.autoRepeat

            onClicked:
                preview.autoRepeat = !preview.autoRepeat
        }
    }
}
