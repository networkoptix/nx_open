// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core.Controls
import Nx.Core

import nx.vms.client.desktop

AbstractButton
{
    id: button

    property bool selected: false

    property bool horizontal: false

    property alias wrapMode: buttonText.wrapMode

    padding: 8
    leftPadding: button.horizontal ? 6 : button.padding
    spacing: button.horizontal ? 4 : 2

    height: button.horizontal ? 28 : 64

    GlobalToolTip.text: buttonText.truncated ? buttonText.text : ""

    background: Rectangle
    {
        id: backgroundRect

        radius: 2

        color:
        {
            if (button.selected)
                return ColorTheme.colors.dark10

            return button.hovered || button.pressed
                ? ColorTheme.colors.dark6
                : ColorTheme.colors.dark5
        }

        border.color: button.selected
            ? ColorTheme.colors.dark13
            : "transparent"
    }

    contentItem: Item
    {
        id: defaultArea //< Area within paddings; actual content may be bigger.

        Item
        {
            id: content

            width: defaultArea.width

            height: button.horizontal
                ? defaultArea.height
                : (iconImage.height + button.spacing + buttonText.implicitHeight)

            anchors.verticalCenter: defaultArea.verticalCenter

            IconImage
            {
                id: iconImage

                source: button.icon.source
                color: buttonText.color
                sourceSize: Qt.size(button.icon.width, button.icon.height)

                x: button.horizontal ? 0 : Math.round((content.width - iconImage.width) / 2)
                y: button.horizontal ? Math.round((content.height - iconImage.height) / 2) : 0
            }

            Text
            {
                id: buttonText

                y: button.horizontal ? 0 : (iconImage.height + button.spacing)
                x: button.horizontal ? (iconImage.width + button.spacing) : 0
                width: content.width - x
                height: content.height - y

                horizontalAlignment: button.horizontal ? Qt.AlignLeft : Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter

                text: button.text
                maximumLineCount: button.horizontal ? 1 : 2
                wrapMode: Text.Wrap
                lineHeight: 15
                lineHeightMode: Text.FixedHeight
                elide: Text.ElideRight

                color: button.selected
                    ? ColorTheme.colors.light1
                    : ColorTheme.colors.light10
            }
        }
    }
}
