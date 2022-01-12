// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.9
import QtQuick.Controls 2.4
import Qt5Compat.GraphicalEffects
import QtQuick.Controls.impl 2.14

import Nx 1.0
import nx.client.desktop 1.0

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

            IconImage
            {
                anchors.verticalCenter: label.verticalCenter
                width: 20
                height: 20

                sourceSize: Qt.size(width, height)

                source: "image://svg/skin/welcome_screen/arrow_down.svg"
                color: control.currentColor
            }
        }
    }

    background: Item {}

    FocusFrame
    {
        anchors.fill: parent
        visible: control.pressed || control.focus
        color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
    }
}
