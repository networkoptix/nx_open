// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4 as T

import Nx 1.0

AbstractButton
{
    id: button

    property color color: ColorTheme.windowText
    property color hoveredColor: ColorTheme.lighter(color, 2)
    property color pressedColor: color

    property alias iconRotation: buttonIcon.rotation
    property alias truncated: buttonText.truncated

    font.pixelSize: 13
    hoverEnabled: true
    background: null
    spacing: text ? 2 : 0

    opacity: enabled ? 1.0 : 0.3

    baselineOffset: contentItem.y + buttonText.y + buttonText.baselineOffset

    contentItem: Item
    {
        id: content

        readonly property real spacing: (buttonIcon.width > 0 && button.text) ? button.spacing : 0

        implicitWidth: buttonIcon.width + buttonText.implicitWidth + content.spacing
        implicitHeight: Math.max(buttonIcon.height, buttonText.implicitHeight)

        T.IconImage
        {
            id: buttonIcon

            name: button.icon.name
            source: button.icon.source
            sourceSize: Qt.size(button.icon.width, button.icon.height)
            visible: !!button.icon.source.toString()
            anchors.verticalCenter: parent.verticalCenter
            color: button.down
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
            color: buttonIcon.color

            x: buttonIcon.width + content.spacing
            width: content.width - x
        }
    }
}
