// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls 2.4

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Control
{
    id: control

    bottomPadding: 8

    property string name: ""
    property string description: ""
    property alias caption: button.text
    property alias value: button.checked
    property bool defaultValue: false

    readonly property bool filled: value

    contentItem: SwitchButton
    {
        id: button
        GlobalToolTip.text: control.description
    }

    function setValue(value)
    {
        control.value = (value === true || value === "true" || value === 1)
    }
}
