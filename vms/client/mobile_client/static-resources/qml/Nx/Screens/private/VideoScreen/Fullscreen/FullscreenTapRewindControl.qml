// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core
import Nx.Core.Controls

Item
{
    id: control

    property int alignment: Qt.AlignLeft
    property alias hintText: indicatorHint.text
    property bool rewindAnimationEnabled: true

    // Width-to-height ratio of the rewind indicator.
    readonly property real indicatorAspectRatio: 2 / 3

    implicitWidth: height * indicatorAspectRatio

    signal activated()
    signal tapped()

    Shape
    {
        id: indicatorBody

        // Keep the indicator proportions when the control is much taller than wide (portrait
        // fullscreen): a lobe at the screen edge instead of a full-height slab.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: Math.min(parent.height, width / control.indicatorAspectRatio)
        opacity: 0

        Behavior on opacity
        {
            NumberAnimation
            {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        ShapePath
        {
            fillColor: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
            strokeWidth: 0

            startX: 0
            startY: 0

            PathLine { x: indicatorBody.width / 3; y: 0 }
            PathArc
            {
                x: indicatorBody.width / 3
                y: indicatorBody.height
                radiusX: indicatorBody.height / 2
                radiusY: indicatorBody.height / 2
            }
            PathLine { x: 0; y: indicatorBody.height }
            PathLine { x: 0; y: 0 }
        }

        Row
        {
            anchors.centerIn: parent

            Repeater
            {
                model: 3
                delegate: ColoredImage
                {
                    id: rewindIcon

                    primaryColor: ColorTheme.colors.light4
                    opacity: rewindAnimationEnabled ? 0 : 0.3

                    sourcePath: "image://skin/24x24/Outline/play_small.svg"
                    sourceSize: Qt.size(24, 24)
                    mirror: true

                    SequentialAnimation
                    {
                        id: rewindAnimation

                        PauseAnimation { duration: 400 - index * 200 }
                        NumberAnimation
                        {
                            duration: 300
                            target: rewindIcon
                            property: "opacity"
                            from: 0
                            to: 1
                        }
                        PauseAnimation { duration: 200 }
                        NumberAnimation
                        {
                            duration: 300
                            target: rewindIcon
                            property: "opacity"
                            from: 1
                            to: 0
                        }
                    }

                    Connections
                    {
                        target: control
                        function onActivated()
                        {
                            if (rewindAnimationEnabled)
                                rewindAnimation.restart()
                        }
                    }
                }
            }
        }

        transform: Scale
        {
            origin: Qt.point(indicatorBody.width / 2, indicatorBody.height / 2)
            xScale: control.alignment === Qt.AlignLeft ? 1 : -1
        }
    }

    Text
    {
        id: indicatorHint

        anchors.centerIn: indicatorBody
        anchors.verticalCenterOffset: 24

        color: ColorTheme.colors.light4
        opacity: indicatorBody.opacity

        font { pixelSize: 16; weight: Font.Normal }
    }

    TapHandler
    {
        gesturePolicy: TapHandler.WithinBounds

        // Taps are counted manually: TapHandler.doubleTapped requires the taps to be close to
        // each other, fires only after the double click interval and gives up on the third quick
        // tap. Here any two quick taps inside the control count, each further quick tap rewinds
        // again, and a single tap is reported once the interval has passed. This lets the user
        // rapidly tap several steps back or forward without pauses between the taps.
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
            ++taps
            restart()

            if (taps < 2)
                return

            control.activated()
            control.showIndicator()
        }
    }

    Timer
    {
        id: hideTimer
        interval: 600
        repeat: false
        onTriggered: indicatorBody.opacity = 0
    }

    function showIndicator()
    {
        indicatorBody.opacity = 1
        hideTimer.restart()
    }
}
