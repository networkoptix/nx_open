// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl as T

import Nx
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

Button
{
    id: button

    property bool isAccentButton: false
    property int contentHAlignment: Qt.AlignHCenter
    readonly property real leftPaddingWithIcon: 4
    readonly property real leftPaddingOnlyText: 16
    property color backgroundColor:
        isAccentButton ? ColorTheme.colors.brand_core : ColorTheme.button

    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor:
        isAccentButton ? ColorTheme.darker(hoveredColor, 1) : backgroundColor
    property color outlineColor: ColorTheme.darker(backgroundColor, 2)

    property color textColor:
        button.isAccentButton ? ColorTheme.colors.brand_contrast : ColorTheme.buttonText

    property real menuXOffset: 0

    property bool showBackground: true

    property var menu: null

    icon
    {
        color: textColor
        width: action ? action.icon.width : 20
        height: action ? action.icon.height : 20
        source: action ? action.icon.source : ""
    }

    leftPadding: icon.source.toString() ? leftPaddingWithIcon : leftPaddingOnlyText
    rightPadding: 16

    spacing: 4

    implicitHeight: 28

    baselineOffset: (contentItem && contentItem.baselineOffset) || 0

    font.pixelSize: FontConfig.normal.pixelSize
    font.weight: Font.Medium

    focusPolicy: Qt.TabFocus

    Keys.enabled: true
    Keys.onEnterPressed: button.clicked()
    Keys.onReturnPressed: button.clicked()

    Binding on opacity
    {
        value: enabled ? 1.0 : (isAccentButton ? 0.2 : 0.3)
    }

    contentItem: Item
    {
        implicitWidth: buttonText.x + buttonText.implicitWidth
        implicitHeight: Math.max(buttonIcon.height, buttonText.implicitHeight)

        baselineOffset: buttonText.y + buttonText.baselineOffset

        Item
        {
            x:
            {
                if (button.contentHAlignment === Qt.AlignHCenter)
                    return (parent.width - width) / 2

                if (button.contentHAlignment === Qt.AlignRight)
                    return parent.width - width

                return 0
            }

            width: Math.min(parent.implicitWidth, parent.width)
            height: parent.height

            ColoredImage
            {
                id: buttonIcon

                name: button.icon.name
                sourcePath: button.icon.source
                sourceSize: Qt.size(button.icon.width, button.icon.height)
                primaryColor: button.textColor
                visible: !!sourcePath

                anchors.verticalCenter: parent.verticalCenter
            }

            T.MnemonicLabel
            {
                id: buttonText

                text: button.text
                font: button.font
                color: button.textColor
                elide: Text.ElideRight

                anchors.verticalCenter: parent.verticalCenter

                width: parent.width - x

                x:
                {
                    if (!buttonIcon.sourcePath)
                        return 0

                    const spacing = button.text ? button.spacing : 0
                    return buttonIcon.width + spacing
                }
            }
        }
    }

    background: ButtonBackground
    {
        hovered: button.hovered && button.enabled
        pressed: button.pressed
        flat: button.flat
        backgroundColor: button.backgroundColor
        hoveredColor: button.hoveredColor
        pressedColor: button.pressedColor
        outlineColor: button.outlineColor

        FocusFrame
        {
            anchors.fill: parent
            anchors.margins: 1
            visible: button.visualFocus
            color: button.isAccentButton ? ColorTheme.brightText : ColorTheme.highlight
        }
    }

    onClicked:
    {
        if (menu instanceof Menu)
        {
            menu.popup(this)
            menu.x = Qt.binding(() => button.menuXOffset)
            menu.y = Qt.binding(() => button.height)
        }
        else if (menu instanceof PlatformMenu)
        {
            menu.open(this)
        }
    }
}
