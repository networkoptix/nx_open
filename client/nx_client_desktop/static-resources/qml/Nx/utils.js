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
