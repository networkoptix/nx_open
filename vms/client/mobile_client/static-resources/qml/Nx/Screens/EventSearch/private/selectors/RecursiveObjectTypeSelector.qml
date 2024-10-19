// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQml.Models 2.15

import Nx.Core 1.0

import nx.vms.client.core.analytics 1.0

import "../editors"

/**
 * Represents full set of object types selector.
 */
Column
{
    id: control

    property var objectTypes: null
    property var value: null

    spacing: 4
    width: (parent && parent.width) ?? implicitWidth

    onValueChanged: d.setValue(value)

    Repeater
    {
        id: repeater

        model: ListModel //< Basic model with unslected object type.
        {
            dynamicRoles: true //< Suppresses empty list element roles.
            Component.onCompleted: d.tryAddFirstSelector()
        }

        delegate: ObjectTypeSelector
        {
            readonly property ObjectTypeSelector parentSelector: index > 0
                ? repeater.itemAt(index - 1)
                : null
            readonly property var derivedTypes: (value && value.derivedObjectTypes) ?? []

            unselectedValue: null
            width: (parent && parent.width) ?? 0
            objectTypes: parentSelector
                ? parentSelector.derivedTypes
                : control.objectTypes
            value: model.selectedValue

            text:
            {
                if (index === 0)
                    return qsTr("Type")

                if (!parentSelector || !parentSelector.value)
                    return ""

                const subtype = qsTr("Subtype")
                return `${parentSelector.value.name} ${subtype}`
            }

            onValueChanged:
            {
                // Removes all underlying items if value is changed.
                if (!d.updating && index >= 0 && index < repeater.model.count - 1)
                    repeater.model.remove(index + 1, repeater.model.count - index - 1)
            }

            onDerivedTypesChanged:
            {
                // Creates new underlying item.
                if (!d.updating && index >= 0 && derivedTypes.length)
                    repeater.model.append({})
            }
        }
    }

    NxObject
    {
        id: d

        property bool updating: false

        property var currentValue:
        {
            if (!repeater.count)
                return null

            const lastIndex = repeater.count - 1
            const lastValue = repeater.itemAt(lastIndex).value
            if (lastValue)
                return lastValue

            return lastIndex > 0
                ? repeater.itemAt(lastIndex - 1).value
                : null
        }

        onCurrentValueChanged: tryUpdateValue()
        onUpdatingChanged: tryUpdateValue()

        function tryUpdateValue()
        {
            if (!updating && control.value !== currentValue)
                control.value = currentValue
        }

        function tryAddFirstSelector()
        {
            if (!repeater.model.count)
                repeater.model.append({selectedValue: null})
        }

        function getNodes(value)
        {
            let nodes = []
            for (let type = value; !!type; type = type.baseObjectType)
                nodes.unshift(type)
            return nodes
        }

        function setValue(value)
        {
            if (currentValue === value)
                return

            updating = true

            let nodes = getNodes(value)

            let i = 0
            while (i < Math.min(nodes.length, repeater.count) && nodes[i] === repeater.itemAt(i).value)
                ++i

            const removeCount = repeater.count - i
            if (removeCount)
                repeater.model.remove(i, removeCount)

            for (; i !== nodes.length; ++i)
                repeater.model.append({selectedValue: nodes[i]})

            if (repeater.count)
            {
                let last = repeater.itemAt(repeater.count - 1)
                if (last.value && last.derivedTypes.length)
                    repeater.model.append({selectedValue: null})
            }

            tryAddFirstSelector()
            updating = false
        }
    }
}
