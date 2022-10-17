// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4

import Nx 1.0

/**
 * TabButton that shows icon and text when selected, and only icon when not selected.
 * The transition is animated.
 *
 * Used palette entries:
 *     ButtonText - normal color
 *     BrightText - hovered color
 *     HighlightedText - selected color
 *     Mid - hover underline color
 */
TabButton
{
    id: tabButton

    property bool secondaryStyle: true

    property bool focusFrameEnabled: true

    property int animationDurationMs: 200

    property real underlineOffset: 0

    property bool compact: true

    readonly property bool isCurrent: TabBar.tabBar && TabBar.index === TabBar.tabBar.currentIndex

    readonly property color color: isCurrent
        ? palette.highlightedText
        : (hovered && !pressed) ? palette.brightText : palette.buttonText

    readonly property color backgroundColor:
    {
        if (secondaryStyle)
            return "transparent"

        if (pressed)
            return ColorTheme.colors.dark9
        if (hovered)
            return ColorTheme.colors.dark11
        return "transparent"
    }

    property alias textHeight: tabText.height
    property alias underlineHeight: underline.height
    property color inactiveUnderlineColor: palette.mid

    leftPadding: secondaryStyle ? 2 : 10
    rightPadding: secondaryStyle ? 2 : 10
    topPadding: 6
    bottomPadding: 6
    spacing: 4

    width: !tabButton.compact || isCurrent ? implicitWidth : (leftPadding + image.width + spacing)
    clip: true

    onActiveFocusChanged: frame.requestPaint()

    contentItem: Item
    {
        id: content

        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row
        {
            id: row

            anchors.verticalCenter: parent.verticalCenter
            spacing: tabButton.spacing

            IconImage
            {
                id: image

                source: tabButton.icon.source
                color: tabButton.color
                name: tabButton.icon.name
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                id: tabText

                text: tabButton.text
                font: tabButton.font
                color: tabButton.color
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Canvas
    {
        id: frame

        readonly property int frameHorizontalPadding: 0
        readonly property int frameVerticalPadding: 0

        onPaint: drawFrame(tabButton.activeFocus)

        function drawFrame(visible)
        {
            if (!tabButton.focusFrameEnabled)
                return

            var ctx = getContext("2d")

            if (!visible)
            {
                ctx.clearRect(0, 0, width, height)
                return
            }

            ctx.setLineDash([1, 1])
            ctx.strokeStyle = setColorAlpha(ColorTheme.colors.brand_core, 0.5)

            ctx.beginPath()
            ctx.moveTo(frameHorizontalPadding, frameVerticalPadding)
            ctx.lineTo(frameHorizontalPadding, height - frameVerticalPadding)
            ctx.lineTo(width - frameHorizontalPadding, height - frameVerticalPadding)
            ctx.lineTo(width - frameHorizontalPadding, frameVerticalPadding)
            ctx.lineTo(frameHorizontalPadding, frameVerticalPadding)
            ctx.stroke()
        }

        function setColorAlpha(color, alpha)
        {
            return Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, alpha)
        }

        Rectangle
        {
            anchors.fill: parent

            color: tabButton.backgroundColor
        }

        Rectangle
        {
            id: underline

            x: content.x
            anchors.bottom: parent.bottom
            // There are some visual bugs with tabButton positioning.
            // Underline is only 1 pixel height instead of 2, without this shift.
            anchors.bottomMargin: (tabButton.secondaryStyle ? 0 : 1) - tabButton.underlineOffset
            width: content.implicitWidth
            height: tabButton.secondaryStyle ? 1 : 2
            visible: !tabButton.compact || tabButton.isCurrent || tabButton.hovered
            color: tabButton.isCurrent ? palette.highlightedText : inactiveUnderlineColor
        }
    }

    Behavior on width
    {
        NumberAnimation
        {
            duration: animationDurationMs
            easing.type: Easing.InOutCubic
        }
    }
}
