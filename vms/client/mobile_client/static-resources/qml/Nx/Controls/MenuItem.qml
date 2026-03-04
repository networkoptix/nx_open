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
    icon.width: 20
    icon.height: 20

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

    contentItem: Item
    {
        implicitWidth: icon.implicitWidth + control.spacing + text.implicitWidth
        visible: control.enabled

        ColoredImage
        {
            id: icon

            anchors.verticalCenter: parent.verticalCenter

            sourcePath: control.icon.source
            sourceSize: Qt.size(control.icon.width, control.icon.height)
            primaryColor: text.color
            visible: !!sourcePath
        }

        Text
        {
            id: text

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: icon.right
            anchors.leftMargin: icon.visible ? control.spacing : 0
            anchors.right: parent.right
            anchors.rightMargin: control.indicator && control.checkable
                ? indicator.width + control.spacing
                : 0

            text: control.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font: control.font
            opacity: enabled ? 1 : 0.3
            color: control.checked ? ColorTheme.colors.dark1 : ColorTheme.colors.light4
        }
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
