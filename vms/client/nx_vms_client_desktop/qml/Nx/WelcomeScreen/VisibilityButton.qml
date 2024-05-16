// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

import nx.vms.client.desktop

Button
{
    id: control

    property color textColor: ColorTheme.colors.light16
    property color pressedTextColor: textColor
    property color hoveredTextColor: ColorTheme.colors.light13

    readonly property color currentColor:
    {
        if (pressed)
            return pressedTextColor

        if (hovered)
            return hoveredTextColor

        return textColor
    }

    leftPadding: 1
    rightPadding: 1
    topPadding: 1
    bottomPadding: 1

    contentItem: Item
    {
        implicitWidth: row.width
        implicitHeight: label.height

        Row
        {
            id: row

            spacing: 4

            Label
            {
                id: label

                height: 16

                leftPadding: 1
                rightPadding: 1

                font: control.font
                text: control.text
                color: control.currentColor
            }

            ColoredImage
            {
                anchors.verticalCenter: label.verticalCenter
                width: 20
                height: 20

                sourceSize: Qt.size(width, height)
                sourcePath: "image://skin/20x20/Outline/arrow_down.svg"
                primaryColor: control.currentColor
            }
        }
    }

    background: Item {}

    FocusFrame
    {
        anchors.fill: parent
        visible: control.pressed || control.focus
    }
}
