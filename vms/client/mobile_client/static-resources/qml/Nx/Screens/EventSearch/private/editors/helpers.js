// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function getValue(index, model, unselectedValue, valueRoleId)
{
    if (index === -1)
        return unselectedValue

    const data = model[index]
    return valueRoleId ? data[valueRoleId] : data
}

function getTextValue(index,
    model,
    unselectedValue,
    unselectedValueText,
    valueRoleId,
    textRoleId,
    valueToTextFunc)
{
    if (valueToTextFunc)
    {
        const value = getValue(index, model, unselectedValue, valueRoleId)
        return value === unselectedValue ? unselectedValueText : valueToTextFunc(value)
    }

    if (index === -1)
        return unselectedValueText

    const data = model[index]
    if (!data)
        return data

    return textRoleId ? data[textRoleId] : data
}

function getIndex(value, model, compareFunc)
{
    const compare = compareFunc
        ? ((val) => compareFunc(val, value))
        : ((val) => val === value)

    return model
        ? model.findIndex(compare)
        : -1
}
