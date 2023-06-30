// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Controls

Control
{
    id: control

    property string permission: ""
    property bool highlighted: false
    property bool collapsed: !highlighted
    property bool collapsible: false

    contentItem: Column
    {
        spacing: 16

        Column
        {
            spacing: 8
            visible: !collapsed || !collapsible

            Text
            {
                text: qsTr("Required permission group")
                font.pixelSize: 14
                color: ColorTheme.light
            }

            Text
            {
                text: `  \u2022  ${permission}`
                font.pixelSize: 14
                color: highlighted ? ColorTheme.colors.yellow_attention : ColorTheme.windowText
            }
        }

        TextButton
        {
            text: control.collapsed ? qsTr("View Permissions") : qsTr("Hide Permissions")
            visible: collapsible

            font.pixelSize: 16
            icon.source: control.collapsed
                ? "image://svg/skin/text_buttons/arrow_down_20.svg"
                : "image://svg/skin/text_buttons/arrow_up_20.svg"

            onClicked: control.collapsed = !control.collapsed
        }
    }
}
