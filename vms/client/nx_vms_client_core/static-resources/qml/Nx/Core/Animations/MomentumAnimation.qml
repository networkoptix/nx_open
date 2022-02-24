// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

PropertyAnimation
{
    id: animation

    /** Maximum absolute value of velocity in units per second. */
    property real maximumVelocity: 1000000

    /** Deceleration in units per second. */
    property real deceleration: 1000

    signal aboutToStart()

    easing.type: Easing.OutQuad

    function restart(velocity)
    {
        stop()

        if (!target)
            return

        aboutToStart()

        const vmod = Math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y)
        const boundedVmod = Math.min(vmod, maximumVelocity)

        const deceleration = Qt.vector2d(
            velocity.x / vmod * this.deceleration, velocity.y / vmod * this.deceleration)

        if (boundedVmod < vmod)
        {
            const multipler = boundedVmod / vmod
            velocity.x *= multipler
            velocity.y *= multipler
        }

        const t = boundedVmod / this.deceleration
        const dx = velocity.x * t - deceleration.x * t * t / 2
        const dy = velocity.y * t - deceleration.y * t * t / 2

        duration = t * 1000

        const p = target[property]
        from = p
        to = Qt.point(p.x + dx, p.y + dy)

        start()
    }
}
