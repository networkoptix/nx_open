import QtQuick 2.6

import Nx 1.0

Rectangle
{
    id: control

    property real maxWidth: 200
    property alias verticalPadding: textItem.topPadding
    property alias horizontalPadding: textItem.leftPadding

    implicitWidth: textItem.width
    implicitHeight: textItem.height

    opacity: 0
    visible: opacity > 0

    radius: 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.8)

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

    Text
    {
        id: textItem

        lineHeight: 16
        lineHeightMode: Text.FixedHeight

        anchors.centerIn: parent

        readonly property FontMetrics fontMetrics: FontMetrics{ font: textItem.font }
        readonly property real textSpace:
            fontMetrics.boundingRect(text).width + leftPadding + rightPadding + 1

        font.pixelSize: 14
        font.weight: Font.Normal
        color: ColorTheme.contrast1

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
            duration: 500
            easing.type: Easing.Linear
        }
    }

    Object
    {
        id: d

        Timer
        {
            id: hideTimer

            repeat: false
            interval: 7000
            onTriggered: control.showText("")
        }
    }

    onOpacityChanged:
    {
        if (opacity == 0)
            textItem.text = ""
    }
}
