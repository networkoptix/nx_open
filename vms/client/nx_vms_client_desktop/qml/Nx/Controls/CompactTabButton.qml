// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

/**
 * TabButton that shows icon and text when selected, and only icon when not selected.
 * The transition is animated.
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

    property color color: ColorTheme.colors.light12
    property color hoveredColor: ColorTheme.colors.light10
    property color highlightedColor: ColorTheme.colors.brand_core

    readonly property color effectiveColor: isCurrent
        ? highlightedColor
        : (hovered && !pressed) ? hoveredColor : color

    property color inactiveUnderlineColor: ColorTheme.colors.dark12

    property color backgroundColor:
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

    leftPadding: secondaryStyle ? 2 : 10
    rightPadding: secondaryStyle ? 2 : 10
    topPadding: 8
    bottomPadding: 8
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
                sourceSize: Qt.size(tabButton.icon.width, tabButton.icon.height)
                color: tabButton.effectiveColor
                name: tabButton.icon.name
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                id: tabText

                text: tabButton.text
                font: tabButton.font
                color: tabButton.effectiveColor
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

            let ctx = getContext("2d")

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
            anchors.bottomMargin: underline.anchors.bottomMargin
            anchors.topMargin: 1
            color: tabButton.backgroundColor
        }

        Rectangle
        {
            id: underline

            x: content.x - 2
            anchors.bottom: parent.bottom
            // There are some visual bugs with tabButton positioning.
            // Underline is only 1 pixel height instead of 2, without this shift.
            anchors.bottomMargin: (tabButton.secondaryStyle ? 0 : 1) - tabButton.underlineOffset
            width: content.implicitWidth + 4
            height: tabButton.secondaryStyle ? 1 : 2
            visible: !tabButton.compact || tabButton.isCurrent || tabButton.hovered

            color: tabButton.isCurrent
                ? tabButton.highlightedColor
                : tabButton.inactiveUnderlineColor
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
