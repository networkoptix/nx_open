// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

import Nx.Core

T.ToolTip
{
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    padding: 12
    topInset: pointer.height
    topPadding: padding + topInset
    timeout: 2000
    closePolicy: T.ToolTip.CloseOnEscape
        |  T.Popup.CloseOnPressOutsideParent
        |  T.Popup.CloseOnReleaseOutsideParent

    background: Item
    {
        Shape
        {
            id: shape

            anchors.fill: parent

            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                fillColor: ColorTheme.colors.light2
                fillRule: ShapePath.WindingFill
                strokeWidth: -1

                PathRectangle
                {
                    radius: 6
                    width: shape.width
                    height: shape.height
                }

                PathPolyline
                {
                    id: pointer

                    property int width: 13
                    property int height: 5
                    property point position: Qt.point(shape.width / 2, -height)

                    path:
                    [
                        Qt.point(position.x - width / 2, position.y + height),
                        Qt.point(position.x, position.y),
                        Qt.point(position.x + width / 2, position.y + height)
                    ]
                }
            }
        }
    }

    contentItem: Text
    {
        text: control.text
        color: ColorTheme.colors.dark4
        font.pixelSize: 14
        elide: Text.ElideRight
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
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
