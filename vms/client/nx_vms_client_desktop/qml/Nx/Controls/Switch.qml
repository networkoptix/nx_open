// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Controls
import Nx.Core

import nx.client.desktop

TextButton
{
    id: button

    property int checkState: Qt.Unchecked

    checkable: true
    spacing: 8

    font.pixelSize: 14

    contentItem: Item
    {
        id: content

        readonly property real spacing: button.text ? button.spacing : 0

        property bool updating: false

        implicitWidth: switchText.implicitWidth + content.spacing + switchIcon.width
        implicitHeight: switchText.implicitHeight

        SwitchIcon
        {
            id: switchIcon

            height: 16
            anchors.verticalCenter: parent.verticalCenter
            checkState: button.checkState
            hovered: button.hovered
        }

        Text
        {
            id: switchText

            x: switchIcon.width + content.spacing
            width: content.width - x
            anchors.verticalCenter: parent.verticalCenter

            font: button.font
            text: button.text
            elide: Text.ElideRight

            color: button.down
                ? button.pressedColor
                : button.hovered ? button.hoveredColor : button.color
        }

        FocusFrame
        {
            anchors.fill: switchText
            anchors.margins: 0
            visible: button.visualFocus
            color: ColorTheme.highlight
            opacity: 0.5
        }
    }

    onCheckStateChanged:
    {
        if (content.updating)
            return

        content.updating = true
        checked = checkState === Qt.Checked
        content.updating = false
    }

    onCheckedChanged:
    {
        if (content.updating)
            return

        content.updating = true
        checkState = checked ? Qt.Checked : Qt.Unchecked
        content.updating = false
    }
}
