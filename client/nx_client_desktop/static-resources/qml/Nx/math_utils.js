function bound(min, value, max)
{
    return Math.max(min, Math.min(max, value))
}

/** @return Size with the same aspect ratio which fits boudingSize. */
function scaledSize(size, boundingSize)
{
    var ar = size.width / size.height
    var boundingAr = boundingSize.width / boundingSize.height

    return ar > boundingAr
        ? Qt.size(boundingSize.width, boundingSize.width / ar)
        : Qt.size(boundingSize.height * ar, boundingSize.height)
}
