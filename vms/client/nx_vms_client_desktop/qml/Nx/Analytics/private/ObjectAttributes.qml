// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import nx.vms.client.desktop.analytics 1.0 as Analytics

Column
{
    id: objectAttributes

    property Analytics.ObjectType objectType
    property string prefix

    property var loggingCategory

    readonly property var selectedAttributeValues: Array.prototype.reduce.call(children,
        function(result, item)
        {
            if (item.selectedValue === undefined)
                return result

            if (item.nestedSelectedValues !== undefined)
            {
                if (Object.keys(item.nestedSelectedValues).length > 0)
                {
                    for (const name in item.nestedSelectedValues)
                        result[`${item.attribute.name}.${name}`] = item.nestedSelectedValues[name]
                }
                else
                {
                    result[item.attribute.name] = item.selectedValue
                        ? (name => `${name}=true`)
                        : (name => `${name}!=true`)
                }
            }
            else
            {
                result[item.attribute.name] = item.selectedValue
            }

            return result
        },
        {})

    function setSelectedAttributeValues(nameValueTree)
    {
        for (let i = 0; i < children.length; ++i)
        {
            const child = children[i]
            if (child instanceof AttributeEditor)
            {
                console.assert(child.attribute)
                if (child.attribute)
                    child.setSelectedValue(nameValueTree[child.attribute.name])
            }
        }
    }

    spacing: 8

    Repeater
    {
        model:
        {
            if (!objectAttributes.objectType)
                return null

            let attributes = objectAttributes.objectType.supportedAttributes
            const derivedTypes = objectAttributes.objectType.derivedTypes

            // Add attributes of private derived types.
            for (let i = 0; i < derivedTypes.length; ++i)
            {
                if (!derivedTypes[i].isPrivate)
                    continue

                attributes = attributes.concat(derivedTypes[i].supportedOwnAttributes)
            }

            return attributes
        }

        AttributeEditor
        {
            id: attributeEditor

            attribute: modelData
            width: objectAttributes.width
            prefix: objectAttributes.prefix
            loggingCategory: objectAttributes.loggingCategory

            contentEnabled: attributeEditor.selectedValue !== undefined
                || objectAttributes.selectedAttributeValues[modelData.name] === undefined
        }
    }
}
