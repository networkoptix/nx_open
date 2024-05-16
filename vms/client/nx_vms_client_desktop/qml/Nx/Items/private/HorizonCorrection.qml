// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Shapes 1.14

import Nx.Core 1.0

Item
{
    id: control

    property real alphaDegrees: 0
    property real betaDegrees: 0

    property color lineColor: ColorTheme.colors.brand_core
    property real lineWidth: 4
    property real meridianLineWidth: 2

    Rectangle
    {
        id: meridian

        color: control.lineColor
        width: control.meridianLineWidth
        height: parent.height
        x: (parent.width - width) * d.meridianPos
    }

    Shape
    {
        id: shape

        // Enable multisampling.
        layer.enabled: true
        layer.samples: 8

        // Rounding is required for good layer mapping.
        width: Math.round(parent.width)
        height: Math.round(parent.height)

        ShapePath
        {
            id: horizonPath

            strokeWidth: control.lineWidth
            strokeColor: control.lineColor

            fillColor: "transparent"

            PathMultiline
            {
                paths:
                {
                    let angles = []
                    if (d.betaDegrees < -89.9 || d.betaDegrees > 89.9)
                    {
                        // 90 degrees rotation is an extreme case.
                        // Rotated horizon becomes vertical.

                        angles = [
                            Qt.vector2d(
                                MathUtils.normalized360(control.alphaDegrees - 90),
                                -d.betaDegrees),

                            Qt.vector2d(
                                MathUtils.normalized360(control.alphaDegrees - 90),
                                d.betaDegrees),

                            Qt.vector2d(
                                MathUtils.normalized360(control.alphaDegrees + 90),
                                d.betaDegrees),

                            Qt.vector2d(
                                MathUtils.normalized360(control.alphaDegrees + 90),
                                -d.betaDegrees)]
                    }
                    else
                    {
                        // Calculate rotated horizon: sample points from the original
                        // straight horizon with 1 degree step, unproject to the unit
                        // sphere, rotate points on the sphere and project back.

                        const beta = MathUtils.toRadians(d.betaDegrees)
                        const sinBeta = Math.sin(beta)
                        const cosBeta = Math.cos(beta)
                        const rotateX = Qt.matrix4x4(1, 0, 0, 0, 0, cosBeta, sinBeta, 0,
                            0, -sinBeta, cosBeta, 0, 0, 0, 0, 1)

                        for (let theta = 0; theta < 360; ++theta)
                        {
                            const pointOnSphere = d.toCarthesian(Qt.vector2d(
                                theta - control.alphaDegrees, 0))

                            const rotated = rotateX.times(pointOnSphere)
                            const newAngles = d.fromCarthesian(rotated)

                            angles.push(Qt.vector2d(MathUtils.normalized360(
                                newAngles.x + control.alphaDegrees), newAngles.y))
                        }
                    }

                    angles.sort(function(l, r){return l.x - r.x})

                    let path = []
                    path.length = angles.length + 2 //< For 2 boundary segments.

                    let i = 0;
                    while (i < angles.length)
                    {
                        const x = angles[i].x / 360.0 * shape.width

                        const y = 1.0 + ((angles[i].y + 90.0) / 180.0
                            * (shape.height - 2.0))

                        path[++i] = Qt.point(x, y)
                    }

                    // Add boundary segments.
                    path[0] = Qt.point(path[i].x - shape.width, path[i].y)
                    path[i + 1] = Qt.point(shape.width + path[1].x, path[1].y)

                    return [path]
                }
            }
        }
    }

    QtObject
    {
        id: d

        readonly property real meridianPos:
            (MathUtils.visualNormalized180(control.alphaDegrees) + 180.0) / 360.0

        readonly property real betaDegrees: MathUtils.bound(
            -90, MathUtils.normalized180(control.betaDegrees), 90)

        function toCarthesian(angles)
        {
            const theta = MathUtils.toRadians(angles.x)
            const phi = MathUtils.toRadians(angles.y)

            const cosPhi = Math.cos(phi)

            return Qt.vector3d(
                cosPhi * Math.sin(theta),
                Math.sin(phi),
                cosPhi * Math.cos(theta))
        }

        function fromCarthesian(coords)
        {
            return Qt.vector2d(
                MathUtils.toDegrees(Math.atan2(coords.x, coords.z)),
                MathUtils.toDegrees(Math.asin(coords.y)))
        }
    }
}
