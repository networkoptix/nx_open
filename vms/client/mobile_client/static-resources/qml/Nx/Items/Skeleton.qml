// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Effects

import Nx.Core

Item
{
    id: skeleton

    default property alias data: skeletonMask.data

    property color color: ColorTheme.colors.dark6
    property real speed: 600 //< Pixels/second

    Rectangle
    {
        id: skeletonBackground

        anchors.fill: parent
        color: skeleton.color
        visible: false

        Rectangle
        {
            id: filler

            width: 180
            height: parent.height

            gradient: Gradient
            {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Qt.rgba(0.12, 0.15, 0.16, 0) }
                GradientStop { position: 0.5; color: Qt.rgba(0.12, 0.15, 0.16, 1) }
                GradientStop { position: 1.0; color: Qt.rgba(0.12, 0.15, 0.16, 0) }
            }

            SequentialAnimation on x
            {
                loops: Animation.Infinite

                PropertyAnimation
                {
                    id: moveAnimation
                    from: -filler.width
                    to: skeletonBackground.width
                    duration: 1000 * (skeletonBackground.width + filler.width) / skeleton.speed
                    easing.type: Easing.Linear
                }

                PauseAnimation
                {
                    duration: moveAnimation.duration * 0.8
                }
            }
        }
    }

    Item
    {
        id: skeletonMask
        anchors.fill: parent
        layer.enabled: true
        visible: false

        // When using Skeleton component, its children serve as a mask through which the skeleton
        // background (with animation) is visible.
        //
        // Example:
        //
        // Rectangle
        // {
        //     radius: 8
        //     color: "white"
        //     anchors.fill: parent
        //     antialiasing: false
        // }
    }

    MultiEffect
    {
        source: skeletonBackground
        maskSource: skeletonMask
        anchors.fill: parent
        maskEnabled: true
    }
}
