// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl

import Nx
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

Button
{
    id: control

    property bool isAccentButton: false
    property var contentHAlignment: Qt.AlignHCenter
    readonly property real leftPaddingWithIcon: 4
    readonly property real leftPaddingOnlyText: 16
    property color backgroundColor:
        isAccentButton ? ColorTheme.colors.brand_core : ColorTheme.button

    property color hoveredColor: ColorTheme.lighter(backgroundColor, 1)
    property color pressedColor:
        isAccentButton ? ColorTheme.darker(hoveredColor, 1) : backgroundColor
    property color outlineColor: ColorTheme.darker(backgroundColor, 2)

    property color textColor:
        control.isAccentButton ? ColorTheme.colors.brand_contrast : ColorTheme.buttonText

    property string iconUrl
    property string hoveredIconUrl
    property string pressedIconUrl
    property alias iconWidth: control.icon.width
    property alias iconHeight: control.icon.height
    property alias iconSpacing: iconLabel.spacing

    property real menuXOffset: 0

    icon.color: textColor
    icon.width: action ? action.icon.width : 20
    icon.height: action ? action.icon.height : 20

    icon.source:
    {
        if (action)
            return action.icon.source

        if (control.pressed)
            return iconLabel.nonEmptyIcon(control.pressedIconUrl, control.iconUrl)

        return (control.hovered && control.hoveredIconUrl.length)
            ? control.hoveredIconUrl
            : control.iconUrl
    }

    property bool showBackground: true

    property var menu: null

    leftPadding: icon.source.toString() ? leftPaddingWithIcon : leftPaddingOnlyText
    rightPadding: 16

    implicitHeight: 28

    font.pixelSize: FontConfig.normal.pixelSize
    font.weight: Font.Medium

    focusPolicy: Qt.TabFocus

    Keys.enabled: true
    Keys.onEnterPressed: control.clicked()
    Keys.onReturnPressed: control.clicked()

    Binding
    {
        target: control
        property: "opacity"
        value: enabled ? 1.0 : (isAccentButton ? 0.2 : 0.3)
    }

    contentItem: Item
    {
        implicitWidth: iconLabel.implicitWidth
        implicitHeight: iconLabel.implicitHeight
        baselineOffset: iconLabel.y + iconLabel.baselineOffset

        IconLabel
        {
            id: iconLabel

            x:
            {
                if (control.contentHAlignment === Qt.AlignHCenter)
                    return (parent.width - width) / 2

                if (control.contentHAlignment === Qt.AlignRight)
                    return parent.width - width

                return 0
            }

            width: Math.min(implicitWidth, parent.width)

            anchors.verticalCenter: parent.verticalCenter

            text: control.text
            icon: control.icon
            font: control.font
            color: control.textColor
            spacing: 4

            function nonEmptyIcon(target, base)
            {
                return target.length ? target : base
            }
        }
    }

    background: ButtonBackground
    {
        hovered: control.hovered && control.enabled
        pressed: control.pressed
        flat: control.flat
        backgroundColor: control.backgroundColor
        hoveredColor: control.hoveredColor
        pressedColor: control.pressedColor
        outlineColor: control.outlineColor

        FocusFrame
        {
            anchors.fill: parent
            anchors.margins: 1
            visible: control.activeFocus
            color: control.isAccentButton ? ColorTheme.brightText : ColorTheme.highlight
            opacity: 0.5
        }
    }

    onClicked:
    {
        if (menu instanceof Menu)
        {
            menu.popup(this)
            menu.x = Qt.binding(() => control.menuXOffset)
            menu.y = Qt.binding(() => control.height)
        }
        else if (menu instanceof PlatformMenu)
        {
            menu.open(this)
        }
    }
}
