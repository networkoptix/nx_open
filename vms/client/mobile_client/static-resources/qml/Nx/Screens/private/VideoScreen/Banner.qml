// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0

Rectangle
{
    id: control

    property real maxWidth: 200
    property alias verticalPadding: textItem.topPadding
    property alias horizontalPadding: textItem.leftPadding
    readonly property alias text: textItem.text
    implicitWidth: textItem.width
    implicitHeight: textItem.height

    opacity: 0
    visible: opacity > 0

    radius: 2
    color: ColorTheme.transparent(ColorTheme.colors.dark8, 0.8)

    function showText(text)
    {
        var hasText = text.length > 0
        if (hasText)
            textItem.text = text

        opacityAnimator.easing.type = hasText ? Easing.InQuad : Easing.OutQuad
        control.opacity = hasText  ? 1 : 0
        if (hasText)
            hideTimer.restart()
        else
            hideTimer.stop()
    }

    function hide()
    {
        hideTimer.stop()
        showText("")
    }

    Text
    {
        id: textItem

        lineHeight: 16
        lineHeightMode: Text.FixedHeight

        anchors.centerIn: parent

        readonly property real textSpace:
            fontMetrics.advanceWidth(text) + leftPadding + rightPadding

        FontMetrics
        {
            id: fontMetrics
            font: textItem.font
        }

        font.pixelSize: 14
        font.weight: Font.Normal
        color: ColorTheme.colors.light1

        width: Math.min(textSpace, control.maxWidth)
        leftPadding: 16
        rightPadding: leftPadding
        topPadding: 12
        bottomPadding: topPadding

        wrapMode: Text.WordWrap
    }

    Behavior on opacity
    {
        NumberAnimation
        {
            id: opacityAnimator
            duration: 100
            easing.type: Easing.Linear
        }
    }

    NxObject
    {
        id: d

        Timer
        {
            id: hideTimer

            repeat: false
            interval: 3000 + opacityAnimator.duration * 2
            onTriggered: control.hide()
        }
    }

    onOpacityChanged:
    {
        if (opacity == 0)
            textItem.text = ""
    }
}
