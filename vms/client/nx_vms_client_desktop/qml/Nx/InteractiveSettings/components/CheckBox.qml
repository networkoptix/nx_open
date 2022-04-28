// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

LabeledItem
{
    id: control

    property alias caption: checkBox.text
    property alias value: checkBox.checked
    property bool defaultValue: false
    property bool isActive: false

    readonly property bool filled: checkBox.checked

    contentItem: Item
    {
        implicitWidth: checkBox.implicitWidth
        implicitHeight: checkBox.implicitHeight
        baselineOffset: checkBox.baselineOffset

        CheckBox
        {
            id: checkBox
            checked: defaultValue
            GlobalToolTip.text: control.defaultValueTooltipText(defaultValue)
        }
    }

    function setValue(value)
    {
        control.value = (value === true || value === "true" || value === 1)
    }
}
