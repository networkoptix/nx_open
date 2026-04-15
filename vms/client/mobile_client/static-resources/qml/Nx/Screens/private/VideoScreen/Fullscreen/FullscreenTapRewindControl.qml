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

    implicitWidth: height * 2 / 3

    signal activated()

    Shape
    {
        id: indicatorBody

        anchors.fill: parent
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

            PathLine { x: width / 3; y: 0 }
            PathArc
            {
                x: width / 3
                y: height
                radiusX: height / 2
                radiusY: height / 2
            }
            PathLine { x: 0; y: height }
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
                    opacity: 0

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
                            rewindAnimation.restart()
                        }
                    }
                }
            }
        }

        transform: Scale
        {
            origin: Qt.point(width / 2, height / 2);
            xScale: control.alignment === Qt.AlignLeft ? 1 : -1
        }
    }

    Text
    {
        id: indicatorHint

        anchors.centerIn: parent
        anchors.verticalCenterOffset: 24

        color: ColorTheme.colors.light4
        opacity: indicatorBody.opacity

        font { pixelSize: 16; weight: Font.Normal }
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onDoubleClicked:
        {
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
