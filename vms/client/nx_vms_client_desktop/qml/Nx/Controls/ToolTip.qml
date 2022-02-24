// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.5
import QtQuick.Controls 2.5
import QtGraphicalEffects 1.0

import Nx 1.0

ToolTip
{
    id: control

    font.pixelSize: 13
    leftPadding: 8
    rightPadding: 8
    topPadding: 2
    bottomPadding: 5
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
