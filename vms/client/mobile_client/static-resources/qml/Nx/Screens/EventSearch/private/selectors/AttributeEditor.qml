// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Mobile.Controls 1.0

import nx.vms.client.core.analytics 1.0

/**
 * Single attribute selector editor. Uses Attribute::Type to determine which selector to show.
 */
Column
{
    id: control

    property Attribute attribute: null
    property string parentPropertyName

    property var value: undefined
    property var unselectedValue

    readonly property bool isObjectAttributesSelector:
        attribute && attribute.type === Attribute.Type.attributeSet

    readonly property OptionSelector selector: content.item

    spacing: 4

    Binding
    {
        target: control
        property: "unselectedValue"
        value: content.item && content.item.unselectedValue
    }

    Loader
    {
        id: content

        width: parent.width

        source:
        {
            if (!control.attribute)
                return ""

            switch (control.attribute.type)
            {
                case Attribute.Type.boolean:
                    return "BooleanSelector.qml"
                case Attribute.Type.string:
                    return "TextSelector.qml"
                case Attribute.Type.number:
                    return "RangeSelector.qml"
                case Attribute.Type.colorSet:
                    return "ColorSelector.qml"
                case Attribute.Type.attributeSet:
                    return "AttributeSetSelector.qml"
                case Attribute.Type.enumeration:
                {
                    const enumeration = control.attribute.enumeration
                    return enumeration && enumeration.items.length
                        ? "EnumerationSelector.qml"
                         : ""
                }
                default:
                    return ""
            }
        }

        Binding
        {
            target: content.item
            property: "text"
            value: control.parentPropertyName
                ? `${control.parentPropertyName} \u2192 ${(control.attribute && control.attribute.name)}`
                : (control.attribute && control.attribute.name)
        }

        Binding
        {
            target: content.item
            property: "screenTitle"
            value:
            {
                if (!content.item)
                    return ""

                return control.attribute && control.attribute.unit
                    ? `${content.item.text}, ${control.attribute.unit}`
                    : content.item.text
            }
        }

        Binding
        {
            target: content.item
            property: "attribute"
            when: content.item && content.item.hasOwnProperty("attribute")
            value: control.attribute
        }

        Binding
        {
            target: content.item
            property: "subpropertyParent"
            when: content.item && content.item.hasOwnProperty("subpropertyParent")
            value: control
        }
    }

    NxObject
    {
        id: d

        Connections
        {
            target: content.item

            function onValueChanged()
            {
                if (!CoreUtils.equalValues(control.value, content.item.value))
                    control.value = content.item.value
            }
        }

        Connections
        {
            target: control

            function onValueChanged()
            {
                if (content.item && !CoreUtils.equalValues(control.value, content.item.value))
                    content.item.value = control.value
            }
        }
    }
}
