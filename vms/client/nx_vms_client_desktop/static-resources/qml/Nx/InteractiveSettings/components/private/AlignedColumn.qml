import QtQuick 2.0

Column
{
    property real labelWidthHint: 0

    readonly property real kLabelWidthFraction: 0.3

    readonly property real labelWidth: labelWidthHint < 64
            ? width * kLabelWidthFraction
            : labelWidthHint

    spacing: 8

    onChildrenChanged: { alignLabels() }
    onLabelWidthChanged: { alignLabels() }

    function alignLabels()
    {
        for (var i = 0; i < children.length; ++i)
        {
            var item = children[i]
            if (item.hasOwnProperty("labelWidth"))
                item.labelWidth = labelWidth
        }
    }
}
