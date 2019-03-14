import QtQuick 2.0

Column
{
    property real maxCaptionWidth: 300

    spacing: 8

    onChildrenChanged: alignCaptions()

    function alignCaptions()
    {
        var maxWidth = 0

        for (var i = 0; i < children.length; ++i)
        {
            var item = children[i]
            if (item.hasOwnProperty("implicitCaptionWidth"))
            {
                item.implicitCaptionWidthChanged.connect(alignCaptions)
                maxWidth = Math.max(maxWidth, item.implicitCaptionWidth)
            }
        }

        maxWidth = Math.min(maxWidth, maxCaptionWidth)

        for (i = 0; i < children.length; ++i)
        {
            item = children[i]
            if (item.hasOwnProperty("captionWidth"))
                item.captionWidth = maxWidth
        }
    }
}
