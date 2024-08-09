// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    id: control

    property var range
    property var value: defaultValue
    property var defaultValue
    property var itemCaptions

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
            if (!NxGlobals.isSequence(range))
                return null

            return range.map(
                function (name) { return (itemCaptions && itemCaptions[name]) || name })
        }

        GlobalToolTip.text: control.defaultValueTooltipText(
            (itemCaptions && itemCaptions[defaultValue]) || defaultValue)
    }
}
