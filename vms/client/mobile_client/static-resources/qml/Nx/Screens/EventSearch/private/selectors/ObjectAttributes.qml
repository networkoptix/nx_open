// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0

import "."
import "../editors"

/**
 * Represents list of object attributes.
 */
Column
{
    id: control

    readonly property bool hasAttributes: repeater.count > 0
    property alias attributes: repeater.model
    property string parentPropertyName

    spacing: 4
    width: (parent && parent.width) ?? 0

    property var value: null

    Repeater
    {
        id: repeater

        delegate: AttributeEditor
        {
            parentPropertyName: control.parentPropertyName
            width: (parent && parent.width) ?? 0
            attribute: modelData
            Component.onCompleted: value = d.getSelectorValue(
                control.value, (attribute && attribute.name) ?? "", control.unselectedValue)
        }
    }

    onValueChanged:
    {
        if (d.updating || CoreUtils.equalValues(value, d.currentValue))
            return

        d.updating = true

        for (let i = 0; i !== repeater.count; ++i)
        {
            const item = repeater.itemAt(i)
            const name = item.attribute.name

            const itemValue = d.getSelectorValue(value, name, item.unselectedValue)
            if (typeof itemValue === "object")
            {
                if (item.value !== true) //< "Present" value in attributes set.
                    item.value = true

                if (item.selector && !CoreUtils.equalValues(item.selector.nestedValue))
                    item.selector.nestedValue = itemValue
            }
            else if (!CoreUtils.equalValues(itemValue, item.value))
            {
                item.value = itemValue
            }

        }
        d.updating = false
    }

    NxObject
    {
        id: d

        property bool updating: false
        property var currentValue:
        {
            let result = {}
            for (let i = 0; i !== repeater.count; ++i)
            {
                const item = repeater.itemAt(i)
                if (CoreUtils.isEmptyValue(item.value) || item.value === item.unselectedValue)
                    continue

                const name = item.attribute.name
                if (!item.isObjectAttributesSelector)
                {
                    result[name] = item.value
                }
                else if (!item.value) // "Absent" value case.
                {
                    result[name] = (name) => `${name}!=true`
                }
                else if (item.selector && item.selector.nestedValue
                    && Object.keys(item.selector.nestedValue).length)
                {
                    result[name] = {}
                    for (let nestedName in item.selector.nestedValue)
                        result[name][nestedName] = item.selector.nestedValue[nestedName]
                }
                else
                {
                    result[name] = item.value
                }
            }
            return result
        }

        onCurrentValueChanged: tryUpdateTargetValue()

        function tryUpdateTargetValue()
        {
            if (updating)
                return

            if (!CoreUtils.equalValues(control.value, currentValue))
                control.value = currentValue
        }

        function getSelectorValue(value, name, unselectedValue)
        {
            return value && value.hasOwnProperty(name)
                ? value[name]
                : unselectedValue
        }
    }
}
