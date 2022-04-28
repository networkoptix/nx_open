// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

LabeledItem
{
    id: control

    property var range
    property var value: defaultValue
    property var defaultValue
    property var itemCaptions
    property bool isActive: false

    defaultValueTooltipEnabled: true

    onValueChanged: comboBox.currentIndex = range.indexOf(value)

    contentItem: ComboBox
    {
        id: comboBox
        onActivated:
        {
            if (control.value !== range[currentIndex])
                control.value = range[currentIndex]
        }

        model:
        {
            if (!Array.isArray(range))
                return null

            return range.map(
                function (name) { return (itemCaptions && itemCaptions[name]) || name })
        }

        GlobalToolTip.text: control.defaultValueTooltipText(
            (itemCaptions && itemCaptions[defaultValue]) || defaultValue)
    }
}
