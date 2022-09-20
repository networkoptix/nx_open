// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import nx.vms.client.desktop 1.0
import nx.vms.client.desktop.analytics 1.0 as Analytics

CollapsiblePanel
{
    id: attributeEditor

    property Analytics.Attribute attribute
    property string prefix: ""
    property var loggingCategory

    readonly property var selectedValue: loader.item
        ? loader.item.selectedValue
        : undefined

    readonly property var nestedSelectedValues: loader.item
        ? loader.item.nestedSelectedValues
        : undefined

    property alias contentEnabled: loader.enabled

    function setSelectedValue(value)
    {
        console.assert(loader.item || value === undefined)
        if (!loader.item)
            return

        function toBoolean(value)
        {
            return value && (typeof value !== "string" || /true/i.test(value))
        }

        if (attribute.type === Analytics.Attribute.Type.attributeSet)
        {
            loader.item.selectedValue = toBoolean(value)
            loader.item.setSelectedAttributeValues(value)
        }
        else if (attribute.type === Analytics.Attribute.Type.boolean)
        {
            loader.item.selectedValue = toBoolean(value)
        }
        else
        {
            loader.item.selectedValue = value
        }
    }

    name:
    {
        if (!attribute)
            return ""

        return attribute.unit
            ? `${prefix}${attribute.name}, ${attribute.unit}`
            : `${prefix}${attribute.name}`
    }

    value: (loader.item && (loader.item.selectedText || loader.item.selectedValue)) || ""

    onClearRequested:
    {
        if (loader && loader.item && loader.item.selectedValue !== undefined)
            loader.item.selectedValue = undefined
    }

    contentItem: Loader
    {
        id: loader

        source:
        {
            if (!attributeEditor.attribute)
                return ""

            switch (attributeEditor.attribute.type)
            {
                case Analytics.Attribute.Type.number:
                    return "RangeEditor.qml"

                case Analytics.Attribute.Type.boolean:
                    return "BooleanRadioGroup.qml"

                case Analytics.Attribute.Type.string:
                    return "StringEditor.qml"

                case Analytics.Attribute.Type.colorSet:
                {
                    if (!attributeEditor.attribute.colorSet
                        || !attributeEditor.attribute.colorSet.items.length)
                    {
                        console.warn(loggingCategory,
                            `Invalid analytics color attribute "${attributeEditor.attribute.id}"`)
                        return ""
                    }

                    if (!ClientSettings.iniConfigValue("compactSearchFilterEditors"))
                        return "ColorRadioGroup.qml"

                    return attributeEditor.attribute.colorSet.items.length < 5
                        ? "ColorTagGroup.qml"
                        : "ColorComboBox.qml"
                }

                case Analytics.Attribute.Type.enumeration:
                {
                    if (!attributeEditor.attribute.enumeration
                        || !attributeEditor.attribute.enumeration.items.length)
                    {
                        console.warn(loggingCategory,
                            `Invalid analytics enum attribute "${attributeEditor.attribute.id}"`)
                        return ""
                    }

                    if (!ClientSettings.iniConfigValue("compactSearchFilterEditors"))
                        return "EnumerationRadioGroup.qml"

                    return attributeEditor.attribute.enumeration.items.length < 5
                        ? "EnumerationTagGroup.qml"
                        : "EnumerationComboBox.qml"
                }

                case Analytics.Attribute.Type.attributeSet:
                    return "ObjectEditor.qml"

                default:
                    console.warn(loggingCategory,
                        `Unknown analytics attribute type "${attributeEditor.attribute.type}"`)
                    return ""
            }
        }

        onLoaded:
        {
            if (item.hasOwnProperty("attribute"))
                item.attribute = attributeEditor.attribute

            if (item.hasOwnProperty("loggingCategory"))
                item.loggingCategory = attributeEditor.loggingCategory

            if (item.hasOwnProperty("container"))
                item.container = attributeEditor
        }
    }
}
