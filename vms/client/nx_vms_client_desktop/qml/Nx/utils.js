// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

.pragma library

function isChild(item, parent)
{
    while (item)
    {
        if (item.parent === parent)
            return true
        item = item.parent
    }
    return false
}

function isRotated90(angle)
{
    return angle % 90 == 0 && angle % 180 != 0
}

function getValue(value, defaultValue)
{
    return value !== undefined ? value : defaultValue
}

function toArray(list)
{
    if (Array.isArray(list))
        return list

    let result = []
    if (!list)
        return result

    for (let i = 0; i < list.length; ++i)
        result.push(list[i])

    return result
}
