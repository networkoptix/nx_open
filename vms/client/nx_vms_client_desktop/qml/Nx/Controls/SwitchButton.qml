// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

Button
{
    id: control

    implicitHeight: 28
    spacing: 16
    font: FontConfig.normal
    rightPadding: 4

    checkable: true

    background: ButtonBackground
    {
        hovered: control.hovered
        pressed: control.pressed
        flat: control.flat
        backgroundColor: ColorTheme.button

        FocusFrame
        {
            anchors.fill: parent
            anchors.margins: 1
            visible: control.visualFocus
            color: ColorTheme.highlight
            opacity: 0.5
        }
    }

    contentItem: Item
    {
        implicitWidth: text.implicitWidth + control.spacing + switchIcon.width
        implicitHeight: text.implicitHeight
        baselineOffset: text.baselineOffset

        Text
        {
            id: text

            width: parent.width - switchIcon.width - control.spacing
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignLeft

            text: control.text
            font: control.font
            color: ColorTheme.buttonText
            elide: Text.ElideRight
        }

        SwitchIcon
        {
            id: switchIcon
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            checkState: control.checked ? Qt.Checked : Qt.Unchecked
            hovered: control.hovered
            uncheckedColor: ColorTheme.colors.dark8
            uncheckedIndicatorColor: ColorTheme.colors.dark10
        }
    }
}
