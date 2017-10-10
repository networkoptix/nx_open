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
