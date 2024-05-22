// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.core
import nx.vms.client.desktop

AbstractButton
{
    id: button

    property color color: ColorTheme.windowText
    property color hoveredColor: ColorTheme.lighter(color, 2)
    property color pressedColor: color

    property alias iconRotation: buttonIcon.rotation
    property alias truncated: buttonText.truncated

    font.pixelSize: FontConfig.normal.pixelSize
    hoverEnabled: enabled
    background: null
    spacing: 2

    opacity: enabled ? 1.0 : 0.3

    baselineOffset: (contentItem && contentItem.baselineOffset) || 0

    focusPolicy: Qt.TabFocus

    icon.width: 20
    icon.height: 20

    contentItem: Item
    {
        implicitWidth: buttonText.x + buttonText.implicitWidth
        implicitHeight: Math.max(buttonIcon.height, buttonText.implicitHeight)

        baselineOffset: buttonText.y + buttonText.baselineOffset

        ColoredImage
        {
            id: buttonIcon

            name: button.icon.name
            sourcePath: button.icon.source
            sourceSize: Qt.size(button.icon.width, button.icon.height)
            visible: !!sourcePath

            anchors.verticalCenter: parent.verticalCenter

            primaryColor: button.down
                ? button.pressedColor
                : button.hovered ? button.hoveredColor : button.color
        }

        Text
        {
            id: buttonText

            font: button.font
            text: button.text
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            color: buttonIcon.primaryColor

            width: parent.width - x

            x:
            {
                if (!buttonIcon.sourcePath)
                    return 0

                const spacing = button.text ? button.spacing : 0
                return buttonIcon.width + spacing
            }
        }

        FocusFrame
        {
            anchors.fill: buttonText
            visible: button.visualFocus
        }
    }
}
