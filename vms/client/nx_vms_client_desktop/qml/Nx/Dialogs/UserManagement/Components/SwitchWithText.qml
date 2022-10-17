// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15
import QtQuick.Layouts 1.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: control

    property alias text: controlText.text
    property alias color: controlText.color

    property bool checked: true

    signal clicked()

    contentItem: Row
    {
        spacing: 8

        SwitchIcon
        {
            id: enabledUserSwitch
            height: 16

            visible: control.enabled

            checkState: control.checked ? Qt.Checked : Qt.Unchecked
            hovered: enabledUserSwitchMouserArea.containsMouse

            MouseArea
            {
                id: enabledUserSwitchMouserArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: control.clicked()
            }
        }

        Text
        {
            id: controlText

            font: Qt.font({pixelSize: 14, weight: Font.Normal})
            height: 16
            color: ColorTheme.colors.light10
        }
    }
}
