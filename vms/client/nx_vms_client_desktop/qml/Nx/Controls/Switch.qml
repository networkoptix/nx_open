// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0

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
