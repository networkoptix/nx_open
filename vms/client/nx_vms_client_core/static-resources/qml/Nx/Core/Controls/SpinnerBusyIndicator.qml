// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Window 2.0
import QtQuick.Controls 2.15

import Nx.Core 1.0

BusyIndicator
{
    id: control

    property real radius: 18
    property real lineWidth: 4
    property alias period: animation.duration

    width: radius * 2
    height: width
    running: visible && opacity != 0

    contentItem: Item
    {
        anchors.fill: parent

        Canvas
        {
            id: canvas

            anchors.centerIn: parent
            width: parent.width * Screen.devicePixelRatio
            height: parent.height * Screen.devicePixelRatio

            renderStrategy: Canvas.Cooperative
            scale: 1.0 / Screen.devicePixelRatio

            onPaint:
            {
                const ctx = canvas.getContext('2d')
                ctx.reset()

                ctx.lineWidth = control.lineWidth * Screen.devicePixelRatio

                const center = width / 2
                const arcRadius = width / 2 - ctx.lineWidth
                const gradient = ctx.createConicalGradient(center, center, 0)
                gradient.addColorStop(0, ColorTheme.colors.light16)
                gradient.addColorStop(1, ColorTheme.colors.dark7)

                ctx.strokeStyle = gradient
                ctx.arc(center, center, arcRadius, 0, 2 * Math.PI, true)
                ctx.stroke()

                ctx.beginPath()
                ctx.fillStyle = ColorTheme.colors.light16
                ctx.arc(center + arcRadius, center, ctx.lineWidth / 2, Math.PI, 0, true)
                ctx.fill()
            }

            NumberAnimation on rotation
            {
                id: animation

                duration: 1500
                from: 0
                to: 360

                loops: Animation.Infinite
                running: control.running
            }
        }
    }
}
