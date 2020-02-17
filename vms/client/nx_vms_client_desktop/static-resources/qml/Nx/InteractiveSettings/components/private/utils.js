function isItemFilled(item)
{
    if (item.hasOwnProperty("filled"))
        return item.filled
    if (item.hasOwnProperty("isFilled"))
        return item.isFilled()
    return true
}
