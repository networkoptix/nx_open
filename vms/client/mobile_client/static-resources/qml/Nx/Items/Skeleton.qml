// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Effects

import Nx.Core

Item
{
    id: skeleton

    default property alias data: skeletonMask.data

    property color color: ColorTheme.colors.dark6
    property color fillerColor: Qt.rgba(0.12, 0.15, 0.16, 1)

    property SkeletonController controller

    Rectangle
    {
        id: skeletonBackground

        anchors.fill: parent
        color: skeleton.color
        visible: false

        Rectangle
        {
            id: filler

            x: skeleton.controller?.fillerPosition ?? 0

            width: skeleton.controller?.fillerWidth ?? 0
            height: parent.height

            readonly property color transparentColor:
                ColorTheme.transparent(skeleton.fillerColor, 0)

            gradient: Gradient
            {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: filler.transparentColor }
                GradientStop { position: 0.5; color: skeleton.fillerColor }
                GradientStop { position: 1.0; color: filler.transparentColor }
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

    Component
    {
        id: defaultController

        SkeletonController
        {
            skeletonWidth: skeleton.width
        }
    }

    Component.onCompleted:
    {
        if (!skeleton.controller)
            skeleton.controller = defaultController.createObject(skeleton)
    }
}
