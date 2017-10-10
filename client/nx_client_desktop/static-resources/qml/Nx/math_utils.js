.pragma library

function bound(min, value, max)
{
    return Math.max(min, Math.min(max, value))
}

function distance(p1, p2)
{
    return Math.sqrt(Math.pow(p1.x - p2.x, 2) + Math.pow(p1.y  - p2.y, 2))
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
