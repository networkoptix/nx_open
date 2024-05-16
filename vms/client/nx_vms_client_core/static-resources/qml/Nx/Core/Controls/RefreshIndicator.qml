// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Window 2.0

import Nx.Core 1.0

Rectangle
{
    id: control

    property real progress: 0
    property color arrowColor: ColorTheme.colors.light1
    property color completeColor: ColorTheme.colors.brand_core
    property alias backgroundColor: control.color

    width: 32
    height: width
    radius: width / 2

    rotation: -90 + 360 * progress
    opacity: progress > 0 ? 1 : 0
    color: ColorTheme.colors.dark8
    onProgressChanged: canvas.requestPaint()

    Canvas
    {
        id: canvas

        anchors.centerIn: parent
        width: parent.width * Screen.devicePixelRatio
        height: parent.height * Screen.devicePixelRatio

        renderStrategy: Canvas.Cooperative
        scale: 1.0 / Screen.devicePixelRatio

        opacity: Math.min(control.progress * 2, 1)

        onPaint:
        {
            const ctx = canvas.getContext('2d')
            ctx.reset()

            const color = control.progress == 1
                ? control.completeColor
                : control.arrowColor

            ctx.strokeStyle = color
            ctx.lineWidth = Screen.devicePixelRatio * 3

            const endAngle = -2 * Math.PI * control.progress * 0.8
            const arcRadius = control.radius * Screen.devicePixelRatio * 0.6
            const center = control.radius * Screen.devicePixelRatio
            ctx.arc(width / 2, height / 2, arcRadius, 0, endAngle, true)
            ctx.stroke()

            const size = 3 * Screen.devicePixelRatio
            const posX = center + arcRadius
            ctx.fillStyle = color
            ctx.beginPath()
            ctx.moveTo(posX - size , center)
            ctx.lineTo(posX, center + size * 2)
            ctx.lineTo(posX + size , center)
            ctx.lineTo(posX - size , center)
            ctx.fill()
        }
    }
}
