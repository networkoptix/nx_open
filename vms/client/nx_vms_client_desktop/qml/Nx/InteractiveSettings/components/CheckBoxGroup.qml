// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

import "private"

/**
 * Interactive Settings type.
 */
LabeledItem
{
    id: control

    isGroup: buttons.count > 1

    property var value: defaultValue
    property var defaultValue
    property var itemCaptions
    property alias range: buttons.model

    readonly property bool filled: value && value !== []

    contentItem: MultiColumn
    {
        id: buttons

        baselineOffset: (count > 0) ? itemAt(0).baselineOffset : 0

        delegate: CheckBox
        {
            readonly property var identifier: modelData

            text: (control.itemCaptions && control.itemCaptions[identifier]) || identifier
            checked: Array.isArray(control.value) && control.value.indexOf(identifier) !== -1

            onClicked:
                buttons.updateValue()

            GlobalToolTip.text: control.defaultValueTooltipText(
                defaultValue.indexOf(identifier) !== -1)
        }

        function updateValue()
        {
            let newValue = []
            for (let i = 0; i !== count; ++i)
            {
                const item = itemAt(i)
                if (item.checked)
                    newValue.push(item.identifier)
            }

            control.value = newValue
        }
    }
}
