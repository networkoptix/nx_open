// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function isItemFilled(item)
{
    if (!item)
        return false
    if (item.hasOwnProperty("filled"))
        return item.filled
    if (item.hasOwnProperty("isFilled"))
        return item.isFilled()
    return true
}
