// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function _findAttributeRecursive(objectType, nameParts)
{
    if (!objectType || nameParts.length < 1)
        return null

    const name = nameParts[0]
    for (let i = 0; i < objectType.attributes.length; ++i)
    {
        const attribute = objectType.attributes[i]
        if (attribute.name == name)
        {
            if (nameParts.length == 1)
                return attribute

            if (attribute.type !== Analytics.Attribute.Type.attributeSet)
            {
                console.warn(`Nested attribute ${attribute.name} is not an attribute set`)
                return null
            }

            return _findAttributeRecursive(attribute.attributeSet, nameParts.slice(1))
        }
    }

    console.warn(`Attribute ${name} was not found in ${objectType}`)
    return null
}

function findAttribute(objectType, attributeName)
{
    if (!objectType)
        return null

    return _findAttributeRecursive(objectType, attributeName.split('.'))
}
