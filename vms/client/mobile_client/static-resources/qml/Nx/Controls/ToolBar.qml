// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.Core

import "."

ToolBarBase
{
    id: toolBar

    property alias leftButtonIcon: leftButton.icon
    property alias leftButtonImageSource: leftButton.imageSource
    property alias rightButtonIcon: rightButton.icon
    property alias title: label.text
    property alias controls: controlsRow.data
    property alias titleOpacity: label.opacity
    property alias titleUnderlineVisible: labelUnderline.visible

    signal leftButtonClicked()
    signal rightButtonClicked()

    IconButton
    {
        id: leftButton
        anchors.verticalCenter: parent.verticalCenter
        x: 8
        padding: 0
        visible: icon.source != ""
        onClicked: toolBar.leftButtonClicked()
        alwaysCompleteHighlightAnimation: false
    }

    PageTitleLabel
    {
        id: label

        readonly property real leftButtonSpace:
            8 + (leftButton.width ? leftButton.x + leftButton.width : 0)
        readonly property real buttonSpace:
            leftButtonSpace + 8 + (rightButton.visible ? rightButton.width + 8 : 0)
        readonly property real maxWidth: parent.width - buttonSpace

        x: Math.max(parent.width / 2 - width / 2, leftButtonSpace)
        width: Math.max(0, Math.min(implicitWidth, maxWidth))
        anchors.verticalCenter: parent.verticalCenter
    }

    Canvas
    {
        id: labelUnderline

        anchors.left: label.left
        anchors.right: label.right
        anchors.top: label.bottom
        anchors.topMargin: 4
        height: 1

        visible: label.visible && label.text != ""

        onPaint:
        {
            const ctx = getContext("2d")

            ctx.strokeStyle = ColorTheme.colors.dark16
            ctx.lineWidth = 1
            ctx.setLineDash([2, 2]);

            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(width, 0);
            ctx.stroke();
        }
    }

    Row
    {
        id: controlsRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: leftButton.right
        anchors.right: rightButton.left
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8
    }

    IconButton
    {
        id: rightButton
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
        padding: 0
        visible: icon.source != ""
        onClicked: toolBar.rightButtonClicked()
        alwaysCompleteHighlightAnimation: false
    }
}
