// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import "private"

MenuItem
{
    id: control

    property alias horizontalAlignment: mainText.horizontalAlignment
    property alias textColor: mainText.color

    property string description: ""
    property string disabledDescription: ""
    property real descriptionSpacing: 2

    property bool showDisabled: false

    font.pixelSize: 16
    leftPadding: 20
    rightPadding: 20
    spacing: 8
    icon.width: 20
    icon.height: 20

    implicitWidth: (enabled || showDisabled)
        ? Math.max(
            background.implicitWidth,
            contentItem.implicitWidth
                + (indicator && control.checkable ? indicator.implicitWidth + spacing : 0)
                + leftPadding + rightPadding)
        : 0
    implicitHeight: (enabled || showDisabled)
        ? Math.max(
            background.implicitHeight,
            contentItem.implicitHeight + topPadding + bottomPadding)
        : 0

    background: Rectangle
    {
        id: menuItemBackground

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
        id: menuItemContent

        implicitWidth: icon.implicitWidth + control.spacing + mainText.implicitWidth

        implicitHeight: mainText.implicitHeight + (descriptionText.text
            ? control.descriptionSpacing + descriptionText.implicitHeight
            : 0)

        visible: control.enabled || control.showDisabled
        opacity: control.enabled ? 1 : 0.3

        ColoredImage
        {
            id: icon

            anchors.verticalCenter: mainText.verticalCenter

            sourcePath: control.icon.source
            sourceSize: Qt.size(control.icon.width, control.icon.height)
            primaryColor: mainText.color
            visible: !!sourcePath
        }

        Text
        {
            id: mainText

            y: Math.round((parent.height - parent.implicitHeight) / 2)

            anchors.left: icon.right
            anchors.leftMargin: icon.visible ? control.spacing : 0
            anchors.right: parent.right
            anchors.rightMargin: control.indicator && control.checkable
                ? indicator.width + control.spacing
                : 0

            text: control.text
            elide: Text.ElideRight
            font: control.font
            color: control.checked ? ColorTheme.colors.dark1 : ColorTheme.colors.light4
        }

        Text
        {
            id: descriptionText

            anchors.top: mainText.bottom
            anchors.topMargin: control.descriptionSpacing
            anchors.left: mainText.left
            anchors.right: mainText.right

            text: control.disabledDescription && !enabled
                ? control.disabledDescription
                : control.description

            wrapMode: Text.Wrap
            font.pixelSize: 14
            color: ColorTheme.colors.light10
            horizontalAlignment: mainText.horizontalAlignment
            visible: !!text
        }
    }

    indicator: ColoredImage
    {
        x: control.width - control.rightPadding - width
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        visible: control.checkable && control.checked

        sourcePath: "image://skin/20x20/Outline/check.svg"
        sourceSize: Qt.size(20, 20)
        primaryColor: mainText.color
    }
}
