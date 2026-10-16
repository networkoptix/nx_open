// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core

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

    TapHandler
    {
        gesturePolicy: TapHandler.WithinBounds

        // Taps are counted manually: TapHandler.doubleTapped requires the taps to be close to
        // each other, fires only after the double click interval and gives up on the third quick
        // tap. Here any two quick taps inside the control start a seek series, and while the
        // indicator is shown every further tap seeks again. A single tap outside of a series is
        // reported once the interval has passed.
        onTapped: (eventPoint) =>
        {
            // WithinBounds does not cancel the gesture on drag, so a swipe inside the control
            // also ends up here. A finger that slid away from the press point is not a tap.
            const dx = eventPoint.position.x - eventPoint.pressPosition.x
            const dy = eventPoint.position.y - eventPoint.pressPosition.y
            if (Math.hypot(dx, dy) > Application.styleHints.startDragDistance)
                return

            tapSequenceTimer.registerTap()
        }
    }

    QtObject
    {
        id: d

        property int seekCount: 0
        property bool indicatorShown: false

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
    }

    Timer
    {
        id: tapSequenceTimer

        property int taps: 0

        interval: Application.styleHints.mouseDoubleClickInterval
        repeat: false

        onTriggered:
        {
            if (taps === 1)
                control.tapped()

            taps = 0
        }

        function registerTap()
        {
            if (d.indicatorShown)
            {
                stop()
                taps = 0
                d.seek()
                return
            }

            ++taps
            restart()

            if (taps >= 2)
                d.seek()
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
}
