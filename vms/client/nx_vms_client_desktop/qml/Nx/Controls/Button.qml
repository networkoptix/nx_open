// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4
import Nx 1.0
import nx.client.desktop 1.0

import "private"

Button
{
    id: control

    property bool isAccentButton: false
    property var contentHAlignment: Qt.AlignHCenter
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

    property PlatformMenu menu: null

    leftPadding: icon.source.toString() ? 4 : 16
    rightPadding: 16

    implicitHeight: 28

    font.pixelSize: 13
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

        IconLabel
        {
            id: iconLabel

            x: control.contentHAlignment === Qt.AlignHCenter
                ? (parent.width - width) / 2
                : control.contentHAlignment === Qt.AlignRight
                    ? parent.width - width
                    : 0
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
        hovered: control.hovered
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
        if (menu)
            menu.open(this)
    }
}
