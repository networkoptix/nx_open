// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import QtQml.Models

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
    property var maxItems

    readonly property bool filled: value && value.length !== 0
    readonly property var valueSet: new Set(value)
    property bool updating: false

    onValueSetChanged:
    {
        updating = true
        for (let i = 0; i < model.count; ++i)
        {
            let item = model.get(i)
            item.checked = valueSet.has(item.value)
        }
        updating = false
    }

    contentItem: MultiSelectionComboBox
    {
        textRole: "caption"
        checkedRole: "checked"
        maxItems: control.maxItems
        focus: true

        model: ListModel
        {
            id: model

            onDataChanged:
            {
                if (control.updating)
                    return

                let result = []
                for (let i = 0; i < model.count; ++i)
                {
                    let item = model.get(i)
                    if (item.checked)
                        result.push(item.value)
                }
                control.value = result
            }
        }
    }

    Component.onCompleted:
    {
        range.forEach(value => model.append({
            value: value,
            caption: (control.itemCaptions && control.itemCaptions[value]) ?? value,
            checked: !!control.value && control.value.includes(value)
        }))
    }
}
