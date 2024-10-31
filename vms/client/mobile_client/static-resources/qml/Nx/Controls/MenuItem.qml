// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

import "private"

MenuItem
{
    id: control

    font.pixelSize: 16
    leftPadding: 16
    rightPadding: 16
    spacing: 16

    implicitWidth: enabled
        ? Math.max(
            background.implicitWidth,
            contentItem.implicitWidth
                + (indicator && control.checkable ? indicator.implicitWidth + spacing : 0)
                + leftPadding + rightPadding)
        : 0
    implicitHeight: enabled
        ? Math.max(
            background.implicitHeight,
            contentItem.implicitHeight + topPadding + bottomPadding)
        : 0

    background: Rectangle
    {
        implicitHeight: 48
        implicitWidth: 48
        color: ColorTheme.colors.dark7

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: control
            highlightColor: control.active ? ColorTheme.colors.brand_core : ColorTheme.colors.dark6
        }
    }

    contentItem: Text
    {
        text: control.text
        x: control.leftPadding
        verticalAlignment: Text.AlignVCenter
        width: control.availableWidth
            - (control.indicator && control.checkable
               ? indicator.width + control.spacing
               : 0)
        elide: Text.ElideRight
        font: control.font
        opacity: enabled ? 1 : 0.3
        color: ColorTheme.windowText
        visible: enabled
    }

    indicator: CheckIndicator
    {
        x: control.width - control.rightPadding - width
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        visible: control.checkable && enabled
        checked: control.checked

        color: ColorTheme.colors.dark5
        checkColor: ColorTheme.colors.brand_core
        checkedColor: ColorTheme.colors.dark7
        backgroundColor: ColorTheme.colors.dark9
        borderWidth: 1
        lineWidth: 1.5
        checkMarkScale: 1.2
        checkMarkHorizontalOffset: 2
        checkMarkVerticalOffset: -1
    }
}
