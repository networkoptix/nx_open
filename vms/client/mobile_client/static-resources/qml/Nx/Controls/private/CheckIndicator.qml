// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Window 2.2
import Nx.Core 1.0

Item
{
    id: control

    property bool checked: false
    property bool removeBorderWhenChecked: true
    property real borderWidth: 2
    property color color: ColorTheme.windowText
    property color checkedColor: ColorTheme.colors.brand_core
    property color checkColor: ColorTheme.colors.brand_contrast
    property color backgroundColor: "transparent"
    property real lineWidth: 2.5
    property real checkMarkScale: 1
    property real checkMarkHorizontalOffset: 0
    property real checkMarkVerticalOffset: 0

    implicitWidth: 18
    implicitHeight: 18

    Rectangle
    {
        id: backgroundRect

        anchors.fill: parent

        color: control.checked
            ? control.checkedColor
            : control.backgroundColor
        border.color: control.checked && removeBorderWhenChecked
            ? control.checkedColor
            : control.color
        border.width: control.borderWidth
        radius: 2

        Canvas
        {
            id: checkMarkItem

            opacity: control.checked ? 1.0 : 0.0

            anchors.centerIn: parent
            anchors.horizontalCenterOffset: control.checkMarkHorizontalOffset
            anchors.verticalCenterOffset: control.checkMarkVerticalOffset
            width: 18 * Screen.devicePixelRatio
            height: 18 * Screen.devicePixelRatio
            scale: 1.0 / Screen.devicePixelRatio * control.checkMarkScale

            renderStrategy: Canvas.Cooperative

            onPaint:
            {
                var ctx = getContext('2d')
                ctx.reset()
                ctx.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

                ctx.lineWidth = control.lineWidth
                ctx.strokeStyle = control.checkColor

                ctx.moveTo(2.5, 8.5)
                ctx.lineTo(7, 12.5)
                ctx.lineTo(15, 4.5)
                ctx.stroke()
            }
        }
    }
}
