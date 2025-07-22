// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.Mobile.Ui
import Nx.Core

import "."

ToolBarBase
{
    id: toolBar

    property alias leftButtonIcon: leftButton.icon
    property alias leftButtonImageSource: leftButton.imageSource
    property alias leftButtonEnabled: leftButton.enabled
    property alias title: label.text
    property alias controls: controlsRow.data
    property alias titleOpacity: label.opacity
    property alias titleUnderlineVisible: labelUnderline.visible

    property alias centerControl: centerControlContainer.data
    property alias rightControl: rightControlContainer.data

    baselineOffset: label.y + label.baselineOffset

    signal leftButtonClicked()

    Item
    {
        id: leftControlContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 20
        width: 32
        height: 32

        IconButton
        {
            id: leftButton
            anchors.centerIn: parent

            padding: 0
            visible: icon.source != ""
            onClicked: toolBar.leftButtonClicked()
            alwaysCompleteHighlightAnimation: false
        }
    }

    Item
    {
        id: centerControlContainer
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: leftControlContainer.right
        anchors.leftMargin: 20
        anchors.right: rightControlContainer.left
        anchors.rightMargin: 20
        height: parent.height

        TitleLabel
        {
            id: label

            readonly property real controlsRowWidth:
                controlsRow.width > 0 ? controlsRow.width + 8 : 0

            readonly property bool fitsInCenter:
                parent.width / 2 + implicitWidth / 2 < parent.width - controlsRowWidth

            readonly property bool hasRightControl: rightControlContainer.children.length > 0

            x: fitsInCenter
                ? Math.max((parent.width - implicitWidth) / 2, 0)
                : Math.max((parent.width - width - controlsRowWidth) / 2, 0)

            width: fitsInCenter
                ? implicitWidth
                : (hasRightControl
                    ? (parent.width - controlsRowWidth)
                    : Math.min(parent.width + 20 + 32, implicitWidth))

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
            onVisibleChanged:
            {
                if (visible)
                    requestPaint()
            }

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
    }

    Row
    {
        // TODO: This item should probably go away
        id: controlsRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: centerControlContainer.right
        spacing: 8
    }

    Item
    {
        id: rightControlContainer
        baselineOffset: toolBar.baselineOffset - y
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 20
        width: 32
        height: 32
    }
}
