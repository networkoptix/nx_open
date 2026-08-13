// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import Nx.Core

Item
{
    id: control

    property vector2d target: Qt.vector2d(0, 0)

    property real radius: 6
    property color color: ColorTheme.colors.light2
    property size arrowSize: Qt.size(13, 5)
    property real arrowMargin: radius

    implicitWidth: 20
    implicitHeight: 20

    Shape
    {
        id: shape

        anchors.fill: parent

        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            simplify: true
            fillColor: control.color
            strokeWidth: -1

            PathRectangle
            {
                radius: control.radius
                width: shape.width
                height: shape.height
            }

            PathPolyline
            {
                id: arrow

                readonly property vector2d tipOffset:
                    d.arrowDirection.times(control.arrowSize.height)

                readonly property vector2d baseOffset:
                    d.arrowTangent.times(control.arrowSize.width / 2)

                path:
                [
                    d.toPoint(d.arrowPosition.minus(baseOffset)),
                    d.toPoint(d.arrowPosition.plus(tipOffset)),
                    d.toPoint(d.arrowPosition.plus(baseOffset))
                ]
            }
        }
    }

    NxObject
    {
        id: d

        property vector2d center: Qt.vector2d(control.width / 2, control.height / 2)
        property vector2d targetDirection: control.target.minus(center)
        property vector2d rectifiedTargetDirection: Qt.vector2d(
            targetDirection.x / (control.width / 2),
            targetDirection.y / (control.height / 2))

        property vector2d arrowDirection:
            Math.abs(rectifiedTargetDirection.x) > Math.abs(rectifiedTargetDirection.y)
                ? Qt.vector2d(Math.sign(rectifiedTargetDirection.x), 0)
                : Qt.vector2d(0, Math.sign(rectifiedTargetDirection.y))

        property vector2d arrowTangent: Qt.vector2d(arrowDirection.y, -arrowDirection.x)
        property vector2d arrowOffset: boundedOffset(
            arrowTangent.times(targetDirection.dotProduct(arrowTangent)),
            control.arrowMargin + control.arrowSize.width / 2)

        property vector2d arrowPosition: Qt.vector2d(
            center.x + arrowDirection.x * control.width / 2 + arrowOffset.x,
            center.y + arrowDirection.y * control.height / 2 + arrowOffset.y)

        function boundedOffset(offset, margin)
        {
            const maxOffsetX = Math.max(0, control.width / 2 - margin)
            const maxOffsetY = Math.max(0, control.height / 2 - margin)

            return Qt.vector2d(
                MathUtils.bound(-maxOffsetX, offset.x, maxOffsetX),
                MathUtils.bound(-maxOffsetY, offset.y, maxOffsetY))
        }

        function toPoint(vector)
        {
            return Qt.point(vector.x, vector.y)
        }
    }
}
