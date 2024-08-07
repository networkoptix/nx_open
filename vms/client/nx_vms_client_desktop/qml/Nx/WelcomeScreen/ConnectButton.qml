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

    text: qsTr("Connect to Server")

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
    topPadding: 2
    bottomPadding: 1

    contentItem: Item
    {
        implicitWidth: row.width
        implicitHeight: row.height

        Row
        {
            id: row

            spacing: 5

            ColoredImage
            {
                width: 15
                height: 15

                sourceSize: Qt.size(width, height)

                sourcePath: "image://skin/20x20/Outline/connect.svg"
                primaryColor: control.currentColor
            }

            Label
            {
                anchors.verticalCenter: parent.verticalCenter
                height: 16

                leftPadding: 1
                rightPadding: 1

                font: control.font
                text: control.text
                color: control.currentColor
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
