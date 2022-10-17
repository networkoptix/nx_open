// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

LimitedFlow
{
    id: control

    spacing: 8

    rowLimit: 2
    items: []

    signal groupClicked(var id)
    signal moreClicked()

    component GroupButton: Button
    {
        id: groupButton

        leftPadding: 12
        rightPadding: 12

        background: Rectangle
        {
            color: (!groupButton.pressed && groupButton.hovered)
                ? ColorTheme.colors.dark10 //< Hovered.
                : groupButton.pressed
                    ? ColorTheme.colors.dark8 //< Pressed.
                    : ColorTheme.colors.dark9 //< Default.

            border.color: (!groupButton.pressed && groupButton.hovered)
                ? ColorTheme.colors.dark11
                : groupButton.pressed
                    ? ColorTheme.colors.dark9
                    : ColorTheme.colors.dark10

            radius: 2
            border.width: 1
        }
    }

    delegate: GroupButton
    {
        icon.source: modelData.isPredefined
            ? "image://svg/skin/user_settings/group_built_in.svg"
            : modelData.isLdap
                ? "image://svg/skin/user_settings/group_ldap.svg"
                : "image://svg/skin/user_settings/group_custom.svg"
        icon.width: 20
        icon.height: 20

        text: modelData.text
        GlobalToolTip.text: modelData.description

        onClicked: control.groupClicked(modelData.id)
    }

    GroupButton
    {
        id: moreGroupsButton

        text: qsTr("and %1 more...").arg(control.remaining)

        onClicked: control.moreClicked()
    }
}
