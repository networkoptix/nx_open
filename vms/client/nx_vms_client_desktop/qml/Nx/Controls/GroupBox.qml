// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

GroupBox
{
    id: control

    leftPadding: 16
    rightPadding: 16
    topPadding: 24
    bottomPadding: 16

    font.pixelSize: FontConfig.normal.pixelSize
    font.weight: Font.Normal

    background: Item
    {
        anchors.fill: parent
        anchors.topMargin: 8

        Rectangle
        {
            id: borderRect

            anchors.fill: parent
            radius: 2
            color: "transparent"
            border.color: ColorTheme.mid
            visible: false
        }

        Item
        {
            id: borderMask

            anchors.fill: borderRect

            Rectangle
            {
                width: 8
                height: 16
            }
            Rectangle
            {
                x: label.x + label.width + 8
                width: parent.width - x
                height: 16
            }
            Rectangle
            {
                anchors.fill: parent
                anchors.topMargin: 16
            }

            visible: false
        }

        OpacityMask
        {
            anchors.fill: borderRect
            source: borderRect
            maskSource: borderMask
        }
    }

    label: Text
    {
        x: control.leftPadding
        width: Math.min(control.availableWidth, implicitWidth)
        text: control.title
        font: control.font
        elide: Text.ElideRight
        color: ColorTheme.darker(ColorTheme.windowText, 4)
    }
}
