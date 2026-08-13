// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Templates as T

import "private"

T.ToolTip
{
    id: control

    required property Item target
    property alias color: shape.color
    property alias radius: shape.radius
    property alias arrowSize: shape.arrowSize
    property int targetSpacing: 0
    readonly property size targetSize: Qt.size(target?.width ?? 0, target?.height ?? 0)
    readonly property rect targetRect: Qt.rect(-x, -y, targetSize.width, targetSize.height)

    parent: target
    x: targetSize.width / 2 - width / 2
    y: targetSize.height + arrowSize.height + targetSpacing

    margins: 6

    implicitWidth: Math.ceil(Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding))
    implicitHeight: Math.ceil(Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding))

    background: BubbleShape
    {
        id: shape

        target: Qt.vector2d(
            control.targetRect.x + control.targetRect.width / 2,
            control.targetRect.y + control.targetRect.height / 2)
    }

    enter: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            from: 0
            to: 1
            duration: 80
        }
    }

    exit: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            from: 1
            to: 0
            duration: 160
        }
    }
}
