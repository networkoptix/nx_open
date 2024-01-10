// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.15
import Qt5Compat.GraphicalEffects

import Nx 1.0
import Nx.Core 1.0

Item
{
    id: bubble

    property int pointerEdge: Qt.LeftEdge

    property real pointerLength: 8
    property real pointerWidth: 16

    property real normalizedPointerPos: 0.5 //< In range [0, 1].

    property real roundingRadius: 2

    property real padding: 8
    property real leftPadding: padding
    property real rightPadding: padding
    property real topPadding: padding
    property real bottomPadding: padding

    property real leftOverhang: 0
    property real rightOverhang: 0
    property real topOverhang: 0
    property real bottomOverhang: 0

    property color color: ColorTheme.colors.dark13
    property color textColor: ColorTheme.colors.light1

    property string text
    property font font

    property color shadowColor: ColorTheme.transparent(ColorTheme.colors.dark1, 0.5)
    property point shadowOffset: Qt.point(0, 3)
    property real shadowBlurRadius: 5

    // If the bubble should contain all decorations (shadows and overhangs) within item bounds.
    // This mode is required for QQuickWidget popup bubbles.
    property bool containDecorationsInside: false

    property alias contentItem: control.contentItem

    property real implicitContentWidth: contentItem ? contentItem.implicitWidth : 0
    property real implicitContentHeight: contentItem ? contentItem.implicitHeight : 0

    readonly property bool horizontal: bubble.pointerEdge === Qt.LeftEdge
        || bubble.pointerEdge === Qt.RightEdge

    readonly property bool vertical: !bubble.horizontal

    /** Local coordinates of the pointer tip. */
    readonly property point pointerPos: Qt.point(
        d.pointerPos.x + boundingRect.left - control.leftInset,
        d.pointerPos.y + boundingRect.top - control.topInset)

    readonly property rect boundingRect:
    {
        if (bubble.containDecorationsInside)
            return Qt.rect(0, 0, bubble.width, bubble.height)

        Qt.rect(
            control.leftInset,
            control.topInset,
            bubble.width - control.leftInset - control.rightInset,
            bubble.height - control.topInset - control.bottomInset)
    }

    readonly property bool effectivelyShown: control.opacity > 0
    visible: effectivelyShown

    property int showDelayMs: 500
    property int fadeInDurationMs: 80

    property int hideDelayMs: 0
    property int fadeOutDurationMs: 80

    function show()
    {
        if (showAnimation.running)
            return

        fadeIn.duration = bubble.fadeInDurationMs * (1.0 - control.opacity)
        hideAnimation.stop()
        showAnimation.start()
    }

    function hide(immediately)
    {
        if (immediately)
        {
            showAnimation.stop()
            hideAnimation.stop()
            control.opacity = 0
        }
        else
        {
            fadeOut.duration = bubble.fadeOutDurationMs * control.opacity
            showAnimation.stop()
            hideAnimation.start()
        }
    }

    /**
     * Position the bubble near specified target in accordance with specified constraints.
     *
     * @param orientation Orientation of the bubble: Qt.Horizontal or Qt.Vertical
     * @param targetRect Rectangle to point the bubble at.
     * @param enclosingRect Rectangle to enclose the bubble within. Bubble shadow may stick out.
     * @param minIntersection Minimum required intersection between targetRect and enclosingRect in
     * the direction orthogonal to orientation; if zero or undefined, half of targetRect is used.
     *
     * @return true if the bubble is positioned or false if it can't satisfy the constraints.
     */
    function pointTo(orientation, targetRect, enclosingRect, minIntersection)
    {
        const parameters = calculateParameters(
            orientation, targetRect, enclosingRect, minIntersection)
        if (!parameters)
            return false

        pointerEdge = parameters.pointerEdge
        normalizedPointerPos = parameters.normalizedPointerPos
        width = implicitWidth
        height = implicitHeight
        x = parameters.x
        y = parameters.y
        return true
    }

    implicitWidth: bubble.containDecorationsInside
        ? control.implicitWidth - control.leftInset - control.rightInset
        : control.implicitWidth

    implicitHeight: bubble.containDecorationsInside
        ? control.implicitHeight - control.topInset - control.bottomInset
        : control.implicitHeight

    Control
    {
        id: control

        anchors.fill: bubble
        anchors.leftMargin: bubble.containDecorationsInside ? -control.leftInset : 0
        anchors.rightMargin: bubble.containDecorationsInside ? -control.rightInset : 0
        anchors.topMargin: bubble.containDecorationsInside ? -control.topInset : 0
        anchors.bottomMargin: bubble.containDecorationsInside ? -control.bottomInset : 0

        // Insets are zero or negative - so actually they're outsets - and contain the shadow.
        leftInset: -Math.max(0, bubble.shadowBlurRadius - bubble.shadowOffset.x) - d.leftOverhang
        rightInset: -Math.max(0, bubble.shadowBlurRadius + bubble.shadowOffset.x) - d.rightOverhang
        topInset: -Math.max(0, bubble.shadowBlurRadius - bubble.shadowOffset.y) - d.topOverhang
        bottomInset: -Math.max(0, bubble.shadowBlurRadius + bubble.shadowOffset.y)
            - d.bottomOverhang

        // Paddings also include the pointer.
        leftPadding: bubble.leftPadding + extraPadding(Qt.LeftEdge)
        rightPadding: bubble.rightPadding + extraPadding(Qt.RightEdge)
        topPadding: bubble.topPadding + extraPadding(Qt.TopEdge)
        bottomPadding: bubble.bottomPadding + extraPadding(Qt.BottomEdge)

        implicitWidth: bubble.implicitContentWidth + leftPadding + rightPadding
        implicitHeight: bubble.implicitContentHeight + topPadding + bottomPadding

        opacity: 0
        layer.enabled: opacity < 1.0

        function extraPadding(edge)
        {
            return edge === bubble.pointerEdge ? bubble.pointerLength : 0
        }

        background: Item
        {
            DropShadow
            {
                x: -control.leftInset
                y: -control.topInset
                width: bubble.width
                height: bubble.height

                color: bubble.shadowColor
                radius: bubble.shadowBlurRadius
                horizontalOffset: bubble.shadowOffset.x
                verticalOffset: bubble.shadowOffset.y

                source: backgroundItem

                Canvas
                {
                    id: backgroundItem

                    visible: false //< Rendered by the effect.

                    width: bubble.width
                    height: bubble.height

                    readonly property rect frame: Qt.rect(
                        control.extraPadding(Qt.LeftEdge),
                        control.extraPadding(Qt.TopEdge),
                        control.width - control.extraPadding(Qt.LeftEdge)
                            - control.extraPadding(Qt.RightEdge),
                        control.height - control.extraPadding(Qt.TopEdge)
                            - control.extraPadding(Qt.BottomEdge))

                    readonly property var edges:
                        [Qt.TopEdge, Qt.RightEdge, Qt.BottomEdge, Qt.LeftEdge]

                    readonly property var corners: [
                        Qt.point(frame.left, frame.top),
                        Qt.point(frame.right, frame.top),
                        Qt.point(frame.right, frame.bottom),
                        Qt.point(frame.left, frame.bottom)]

                    readonly property var edgePoints: [
                        computeEdgePoints(0),
                        computeEdgePoints(1),
                        computeEdgePoints(2),
                        computeEdgePoints(3)]

                    readonly property var updateTriggers: [
                        edgePoints,
                        bubble.color]

                    onUpdateTriggersChanged:
                        requestPaint()

                    function computeEdgePoints(index)
                    {
                        let points = []
                        const edge = edges[index]

                        const next = (index + 1) % edges.length
                        const length = (index & 1) ? frame.height : frame.width
                        const vector = MathUtils.pointSub(corners[next], corners[index])
                        const dir = MathUtils.pointDiv(vector, length)
                        const radius = bubble.roundingRadius

                        if (edge != bubble.pointerEdge)
                        {
                            points.push(MathUtils.pointAdd(corners[index],
                                MathUtils.pointMul(dir, radius)))

                            points.push(MathUtils.pointSub(corners[next],
                                MathUtils.pointMul(dir, radius)))
                        }
                        else
                        {
                            const pointerPos = length * (index < 2
                                ? bubble.normalizedPointerPos
                                : (1.0 - bubble.normalizedPointerPos))

                            const pointerStart = Math.max(pointerPos - d.halfPointerWidth, 0)
                            const pointerEnd =
                                Math.min(pointerPos + d.halfPointerWidth, length)

                            if (pointerStart > radius)
                            {
                                points.push(MathUtils.pointAdd(corners[index],
                                    MathUtils.pointMul(dir, radius)))
                            }

                            points.push(MathUtils.pointAdd(corners[index],
                                MathUtils.pointMul(dir, pointerStart)))

                            points.push(d.pointerPos)

                            points.push(MathUtils.pointAdd(corners[index],
                                MathUtils.pointMul(dir, pointerEnd)))

                            if (pointerEnd < length - radius)
                            {
                                points.push(MathUtils.pointSub(corners[next],
                                    MathUtils.pointMul(dir, radius)))
                            }
                        }

                        return points
                    }

                    onPaint:
                    {
                        const context = getContext("2d")
                        context.reset()
                        context.fillStyle = bubble.color

                        const firstPoint = edgePoints[0][0]
                        context.moveTo(firstPoint.x, firstPoint.y)

                        for (let index = 0; index < edges.length; ++index)
                        {
                            const points = edgePoints[index]
                            for (let i = 1; i < points.length; ++i)
                                context.lineTo(points[i].x, points[i].y)

                            const next = (index + 1) % edges.length
                            const nextPoint = edgePoints[next][0]
                            const corner = corners[next]

                            const radius = Math.min(bubble.roundingRadius,
                                MathUtils.distance(points[points.length - 1], corner),
                                MathUtils.distance(nextPoint, corner))

                            context.arcTo(corner.x, corner.y, nextPoint.x, nextPoint.y, radius)
                        }

                        context.closePath()
                        context.fill()
                    }
                }
            }
        }

        contentItem: Text
        {
            text: bubble.text
            font: bubble.font
            color: bubble.textColor
        }
    }

    NxObject
    {
        id: d

        readonly property real leftOverhang: Math.max(0, bubble.leftOverhang)
        readonly property real rightOverhang: Math.max(0, bubble.rightOverhang)
        readonly property real topOverhang: Math.max(0, bubble.topOverhang)
        readonly property real bottomOverhang: Math.max(0, bubble.bottomOverhang)

        readonly property real halfPointerWidth: bubble.pointerWidth / 2

        // Position of the pointer tip relative to control item.
        readonly property point pointerPos:
        {
            const pos = MathUtils.bound(0.0, bubble.normalizedPointerPos, 1.0)

            switch (bubble.pointerEdge)
            {
                case Qt.RightEdge:
                    return Qt.point(control.width, control.height * pos)

                case Qt.TopEdge:
                    return Qt.point(control.width * pos, 0)

                case Qt.BottomEdge:
                    return Qt.point(control.width * pos, control.height)

                default:
                    return Qt.point(0, control.height * pos)
            }
        }

        readonly property real pointerShift:
        {
            switch (bubble.pointerEdge)
            {
                case Qt.RightEdge:
                    return -calculatePointerShift(d.pointerPos.y, backgroundItem.height)

                case Qt.TopEdge:
                    return -calculatePointerShift(d.pointerPos.x, backgroundItem.width)

                case Qt.BottomEdge:
                    return calculatePointerShift(d.pointerPos.x, backgroundItem.width)

                default:
                    return calculatePointerShift(d.pointerPos.y, backgroundItem.height)
            }
        }

        function calculatePointerShift(pos, size)
        {
            const threshold = d.halfPointerWidth + bubble.roundingRadius
            if (pos < threshold)
                return pos - threshold

            if (pos > size - threshold)
                return pos - (size - threshold)

            return 0
        }
    }

    /**
     * Calculate parameters for bubble positioning near specified target in accordance with
     * specified constraints. This function assumes the bubble is allowed to take up the space
     * dictated by contentItem implicit size.
     *
     * @param orientation Orientation of the bubble pointer: Qt.Horizontal or Qt.Vertical
     * @param targetRect Rectangle to point the bubble at.
     * @param enclosingRect Rectangle to enclose the bubble within. Bubble shadow may stick out.
     * @param minIntersection Minimum required intersection between targetRect and enclosingRect in
     * the direction orthogonal to orientation; if zero or undefined, half of targetRect is used.
     *
     * @return undefined if the bubble can't satisfy the constraints, otherwise a JS object with
     * the following properties (semantically matching the bubble's properties of the same name):
     * pointerEdge, normalizedPointerPos, x and y.
     */
    function calculateParameters(orientation, targetRect, enclosingRect, minIntersection)
    {
        if (!enclosingRect || !enclosingRect.width || !enclosingRect.height)
        {
            enclosingRect = Qt.rect(-Number.MAX_VALUE / 2, -Number.MAX_VALUE / 2,
                Number.MAX_VALUE, Number.MAX_VALUE)
        }

        if (orientation === Qt.Horizontal)
        {
            const requiredWidth = bubble.implicitContentWidth
                + bubble.leftPadding + bubble.rightPadding + bubble.pointerLength

            const requiredHeight = bubble.implicitContentHeight
                + bubble.topPadding + bubble.bottomPadding

            const minTargetY = Math.max(targetRect.top, enclosingRect.top)
            const maxTargetY = Math.min(targetRect.bottom, enclosingRect.bottom)

            if (minIntersection)
                minIntersection = Math.min(minIntersection, targetRect.height)
            else
                minIntersection = targetRect.height / 2

            // If the target is outside of the enclosing range...
            if (minIntersection > 0 && maxTargetY - minTargetY < minIntersection)
                return undefined

            const rightSpace = enclosingRect.right - targetRect.right
            const leftSpace = targetRect.left - enclosingRect.left

            // If the bubble doesn't fit horizontally...
            if (Math.max(rightSpace, leftSpace) < requiredWidth)
                return undefined

            const selectedEdge = requiredWidth <= rightSpace ? Qt.LeftEdge : Qt.RightEdge
            const leftShift = bubble.containDecorationsInside ? control.leftInset : 0

            const bubbleX = selectedEdge === Qt.LeftEdge
                ? targetRect.right + leftShift
                : targetRect.left - requiredWidth + leftShift

            const pointToY = (minTargetY + maxTargetY) / 2.0
            const desiredY = pointToY - requiredHeight / 2.0

            const bubbleY_noShadow = Math.min(Math.max(enclosingRect.top, desiredY),
                enclosingRect.bottom - requiredHeight)

            if (bubbleY_noShadow < enclosingRect.top) //< If the bubble doesn't fit vertically.
                return undefined

            const topShift = bubble.containDecorationsInside ? control.topInset : 0

            return {
                "pointerEdge": selectedEdge,
                "normalizedPointerPos": (pointToY - bubbleY_noShadow) / requiredHeight,
                "x": bubbleX,
                "y": bubbleY_noShadow + topShift}
        }
        else
        {
            const requiredWidth = bubble.implicitContentWidth
                + bubble.leftPadding + bubble.rightPadding

            const requiredHeight = bubble.implicitContentHeight
                + bubble.topPadding + bubble.bottomPadding + bubble.pointerLength

            const minTargetX = Math.max(targetRect.left, enclosingRect.left)
            const maxTargetX = Math.min(targetRect.right, enclosingRect.right)

            if (minIntersection)
                minIntersection = Math.min(minIntersection, targetRect.width)
            else
                minIntersection = targetRect.width / 2

            // If the target is outside of the enclosing range...
            if (minIntersection > 0 && maxTargetX - minTargetX < minIntersection)
                return undefined

            const bottomSpace = enclosingRect.bottom - targetRect.bottom
            const topSpace = targetRect.top - enclosingRect.top

            // If the bubble doesn't fit vertically...
            if (Math.max(bottomSpace, topSpace) < requiredHeight)
                return undefined

            const selectedEdge = requiredHeight <= topSpace ? Qt.BottomEdge : Qt.TopEdge
            const topShift = bubble.containDecorationsInside ? control.topInset : 0

            const bubbleY = selectedEdge === Qt.TopEdge
                ? targetRect.bottom + topShift
                : targetRect.top - requiredHeight + topShift

            const pointToX = (minTargetX + maxTargetX) / 2.0
            const desiredX = pointToX - requiredWidth / 2.0

            const bubbleX_noShadow = Math.min(Math.max(enclosingRect.left, desiredX),
                enclosingRect.right - requiredWidth)

            if (bubbleX_noShadow < enclosingRect.left) //< If the bubble doesn't fit horizontally.
                return undefined

            const leftShift = bubble.containDecorationsInside ? control.leftInset : 0

            return {
                "pointerEdge": selectedEdge,
                "normalizedPointerPos": (pointToX - bubbleX_noShadow) / requiredWidth,
                "x": bubbleX_noShadow + leftShift,
                "y": bubbleY}
        }
    }

    SequentialAnimation
    {
        id: showAnimation

        PauseAnimation
        {
            duration: (control.opacity > 0) ? 0 : bubble.showDelayMs
        }

        NumberAnimation
        {
            id: fadeIn

            target: control
            property: "opacity"
            to: 1
        }
    }

    SequentialAnimation
    {
        id: hideAnimation

        PauseAnimation
        {
            duration: (control.opacity < 1) ? 0 : bubble.hideDelayMs
        }

        NumberAnimation
        {
            id: fadeOut

            target: control
            property: "opacity"
            to: 0
        }
    }
}
