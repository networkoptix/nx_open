// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import "private"

MenuItem
{
    id: control

    property alias horizontalAlignment: text.horizontalAlignment
    property alias textColor: text.color

    font.pixelSize: 16
    leftPadding: 20
    rightPadding: 20
    spacing: 8

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
        color: control.checked ? ColorTheme.colors.brand : ColorTheme.colors.dark14

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: control
            highlightColor:
                control.active ? ColorTheme.colors.brand_core : ColorTheme.colors.dark11
        }
    }

    contentItem: Text
    {
        id: text

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
        color: control.checked ? ColorTheme.colors.dark1 : ColorTheme.colors.light4
        visible: enabled
    }

    indicator: ColoredImage
    {
        x: control.width - control.rightPadding - width
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        visible: control.checkable && control.checked

        sourcePath: "image://skin/20x20/Outline/check.svg"
        sourceSize: Qt.size(20, 20)
        primaryColor: text.color
    }
}
