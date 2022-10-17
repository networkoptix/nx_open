// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

SwitchWithText
{
    id: control

    text: control.checked ? qsTr("Enabled user") : qsTr("Disabled user")
    color: control.checked
        ? ColorTheme.colors.green_core
        : ColorTheme.colors.light16

    onClicked: control.checked = !control.checked
}
