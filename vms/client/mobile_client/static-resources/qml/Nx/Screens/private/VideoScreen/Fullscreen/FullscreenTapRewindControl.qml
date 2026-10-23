// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Handlers

Item
{
    id: control

    property int alignment: Qt.AlignLeft
    property alias hintText: indicatorHint.text

    // Seeking in this direction is impossible (e.g. forward in live): the arrows are dimmed and
    // not animated.
    property bool seekBlocked: false

    // Width of the indicator attached to the outer edge of the control.
    property real indicatorWidth: width

    // Maximum delay between the release of a tap and the press of the next one for the taps to
    // make a sequence. Every tap of a sequence but the first one seeks. While the indicator is
    // shown, the sequence goes on regardless of the delay.
    property int doubleTapIntervalMs: 250

    // Number of seeks in the current series. A new series starts when the indicator is hidden.
    readonly property alias seekCount: d.seekCount

    // Whether the indicator is shown, i.e. a seek series is in progress.
    readonly property alias indicatorShown: d.indicatorShown

    // Width of the tap zone. The indicator inside it has its own width.
    implicitWidth: height * 2 / 3

    signal activated()
    signal tapped()

    Rectangle
    {
        id: indicatorBody

        readonly property real innerRadius: width / 2

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: control.alignment === Qt.AlignLeft ? 0 : control.width - width
        width: control.indicatorWidth

        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.5)
        topLeftRadius: control.alignment === Qt.AlignLeft ? 0 : innerRadius
        bottomLeftRadius: topLeftRadius
        topRightRadius: control.alignment === Qt.AlignLeft ? innerRadius : 0
        bottomRightRadius: topRightRadius

        opacity: d.indicatorShown ? 1 : 0
        visible: opacity > 0

        Behavior on opacity
        {
            id: fadeBehavior

            NumberAnimation
            {
                duration: fadeBehavior.targetValue > 0 ? 150 : 200
                easing.type: Easing.OutQuad
            }
        }

        // Content is centered in the part of the indicator not covered by the system insets.
        Item
        {
            id: contentArea

            anchors.fill: parent
            anchors.leftMargin: indicatorBody.SafeArea.margins.left
            anchors.rightMargin: indicatorBody.SafeArea.margins.right

            Column
            {
                anchors.centerIn: parent
                width: parent.width
                spacing: 8

                Row
                {
                    id: arrows

                    property int step: 0

                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 6
                    rotation: control.alignment === Qt.AlignLeft ? 180 : 0

                    Repeater
                    {
                        model: 3

                        delegate: Shape
                        {
                            // Cascade: the brightest arrow moves forward by half an arrow per
                            // step, so each arrow goes 0 -> 0.5 -> 1 -> 0.5 -> 0 over the loop.
                            readonly property real cascadeOpacity:
                                Math.max(0, 1 - Math.abs(arrows.step - index - 2) / 2)

                            width: 14
                            height: 16
                            preferredRendererType: Shape.CurveRenderer
                            opacity: control.seekBlocked ? 0.3 : cascadeOpacity

                            Behavior on opacity
                            {
                                enabled: !control.seekBlocked

                                NumberAnimation
                                {
                                    duration: cascadeTimer.interval
                                    easing.type: Easing.OutQuad
                                }
                            }

                            ShapePath
                            {
                                fillColor: ColorTheme.colors.light4
                                strokeWidth: -1

                                PathSvg
                                {
                                    path: "M0 2.585C0 1.433 1.244 0.711 2.244 1.282L11.721 6.698"
                                        + "C12.729 7.273 12.729 8.727 11.721 9.302L2.244 14.718"
                                        + "C1.244 15.289 0 14.567 0 13.415V2.585Z"
                                }
                            }
                        }
                    }

                    Timer
                    {
                        id: cascadeTimer

                        interval: 200
                        repeat: true
                        running: indicatorBody.visible && !control.seekBlocked

                        // Start from the step where the first arrow is already visible.
                        onRunningChanged: arrows.step = 1
                        onTriggered: arrows.step = (arrows.step + 1) % 6
                    }
                }

                Text
                {
                    id: indicatorHint

                    width: parent.width
                    leftPadding: 12
                    rightPadding: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap

                    color: ColorTheme.colors.light4
                    font { pixelSize: 14; weight: Font.Normal }
                }
            }
        }
    }

    // Passive, so the gestures still reach the video under the control.
    PassiveTapHandler
    {
        id: tapHandler

        onTapped: (eventPoint) =>
        {
            const inSequence = d.indicatorShown
                || eventPoint.pressTimestamp - d.lastTapTimestamp < control.doubleTapIntervalMs

            if (!inSequence)
                d.finishSequence()

            d.pendingSingleTap = !inSequence
            d.lastTapTimestamp = eventPoint.timestamp
            sequenceTimer.restart()

            if (inSequence)
                d.seek()
        }
    }

    QtObject
    {
        id: d

        property int seekCount: 0
        property bool indicatorShown: false

        // Whether the sequence has only one tap so far, and the release time of its last tap.
        property bool pendingSingleTap: false
        property real lastTapTimestamp: 0

        function seek()
        {
            // The count is not reset on hide, so the label does not change while fading out.
            if (!indicatorShown)
                seekCount = 0

            if (!control.seekBlocked)
                ++seekCount

            // Show the indicator before notifying, so the series never looks finished in between.
            control.showIndicator()
            control.activated()
        }

        function finishSequence()
        {
            sequenceTimer.stop()

            if (pendingSingleTap)
                control.tapped()

            pendingSingleTap = false
            lastTapTimestamp = 0
        }
    }

    Timer
    {
        id: sequenceTimer

        // Whether a tap belongs to the sequence is decided by the event timestamps. The timer
        // only ends the sequence, so it gets a margin of a few frames to never fire before a late
        // delivered tap that is still within the window.
        interval: control.doubleTapIntervalMs + 50
        repeat: false

        onTriggered:
        {
            // The next tap may be still pressed: its release decides, or the next check if it
            // never becomes a tap.
            if (tapHandler.active)
                restart()
            else
                d.finishSequence()
        }
    }

    Timer
    {
        id: hideTimer

        interval: 800
        repeat: false

        onTriggered: control.hideIndicator()
    }

    function showIndicator()
    {
        // Restart the cascade also when the indicator is shown again while still fading out.
        if (!d.indicatorShown)
            arrows.step = 1

        d.indicatorShown = true
        hideTimer.restart()
    }

    function hideIndicator()
    {
        hideTimer.stop()
        d.indicatorShown = false
    }

    // Drops the current tap sequence without reporting a pending single tap.
    function cancelTaps()
    {
        sequenceTimer.stop()
        d.pendingSingleTap = false
        d.lastTapTimestamp = 0
    }
}
