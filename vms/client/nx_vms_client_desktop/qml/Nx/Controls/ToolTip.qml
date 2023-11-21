// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import Nx
import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

ToolTip
{
    id: control

    font.pixelSize: FontConfig.normal.pixelSize
    leftPadding: 8
    rightPadding: 8
    topPadding: 4
    bottomPadding: 4
    margins: 0

    delay: 500

    property real maxWidth: 500

    property color color: ColorTheme.transparent(ColorTheme.colors.dark4, 0.35)
    property color textColor: ColorTheme.colors.dark4

    contentWidth: Math.min(implicitContentWidth, Math.max(maxWidth - leftPadding - rightPadding, 0))

    background: Item
    {
        Rectangle
        {
            id: backgroundRect
            anchors.fill: parent
            color: ColorTheme.text
            radius: 2
            visible: false
        }

        DropShadow
        {
            anchors.fill: parent
            radius: 5
            verticalOffset: 3
            source: backgroundRect
            color: control.color
        }
    }

    contentItem: Text
    {
        text: control.text
        font: control.font
        color: control.textColor
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        textFormat: Text.RichText //< For some reason Text.AutoText works very unreliable.
        wrapMode: Text.WordWrap
    }

    enter: Transition
    {
        // Fade in.
        NumberAnimation
        {
            property: "opacity"
            to: 1
            duration: 150
        }
    }

    exit: Transition
    {
        SequentialAnimation
        {
            // Ensure that the fade in is finished.
            NumberAnimation
            {
                property: "opacity"
                to: 1
                duration: 100
            }

            PauseAnimation { duration: 200 }

            // Fade out.
            NumberAnimation
            {
                property: "opacity"
                to: 0
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
    }

    property Item invoker //< Used by GlobalToolTip.
}
