// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

Item
{
    id: control

    property alias checked: clickableSwitch.checked

    implicitWidth: control.enabled ? clickableSwitch.implicitWidth : readonlyText.implicitWidth
    implicitHeight: clickableSwitch.implicitHeight

    Switch
    {
        id: clickableSwitch

        visible: control.enabled
        text: control.checked ? qsTr("Enabled user") : qsTr("Disabled user")

        color: control.checked
            ? ColorTheme.colors.green_core
            : ColorTheme.colors.light16
    }

    Text
    {
        id: readonlyText

        visible: !control.enabled
        font: clickableSwitch.font
        text: clickableSwitch.text
        color: clickableSwitch.color
        anchors.baseline: clickableSwitch.baseline
    }
}
