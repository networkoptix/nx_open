import QtQuick 2.0

import Nx 1.0

UnrotatedArea
{
    id: control

    property bool prefferTopLeftPosition: false
    property alias label: textItem
    property Item boundingItem: null

    Text
    {
        id: textItem

        x: d.position.x
        y: d.position.y
    }

    UnrotatedArea
    {
        id: boundArea

        parent: control.boundingItem ? control.boundingItem : parent
        revertRotationAngle: control.revertRotationAngle
    }

    function update()
    {
        d.topLeft = d.getTopLeft()
        d.bottomRight = d.getBottomRight()
    }

    Object
    {
        id: d

        Binding
        {
            target: d
            property: "topLeft"
            value: d.getTopLeft()
        }

        Binding
        {
            target: d
            property: "bottomRight"
            value: d.getBottomRight()
        }

        property point topLeft: getTopLeft()
        property point bottomRight: getBottomRight()


        readonly property real offset: 8
        readonly property bool enoughWidth: control.width > textItem.width + offset * 2
        readonly property bool enoughHeight: control.height > textItem.height + offset * 2

        readonly property real minHorizontalOffset: textItem.width + offset
        readonly property bool enoughLeftOffset: topLeft.x > minHorizontalOffset
        readonly property bool enoughRightOffset: boundArea.width - bottomRight.x > minHorizontalOffset

        readonly property real minVerticalOffset: textItem.height + offset
        readonly property bool enoughTopOffset: topLeft.y > minVerticalOffset
        readonly property bool enoughBottomOffset: boundArea.height - bottomRight.y > minVerticalOffset

        readonly property point position:
        {
            if (control.prefferTopLeftPosition)
            {
                if (d.enoughWidth)
                {
                    if (d.enoughTopOffset)
                        return Qt.point(0, -d.minVerticalOffset)

                    if (d.enoughHeight && d.enoughWidth)
                        return Qt.point(d.offset, d.offset)

                    if (d.enoughBottomOffset)
                        return Qt.point(0, control.height + d.offset)
                }

                return d.enoughLeftOffset
                    ? Qt.point(-d.minHorizontalOffset, 0)
                    : Qt.point(control.width + d.offset, 0)
            }

            if (d.enoughWidth)
            {
                var prefferedX = control.width - textItem.width
                if (d.enoughBottomOffset)
                    return Qt.point(prefferedX, control.height + d.offset)

                if (d.enoughHeight && d.enoughWidth)
                    return Qt.point(prefferedX - d.offset, control.height - d.minVerticalOffset)

                if (d.enoughTopOffset)
                    return Qt.point(prefferedX, -d.minVerticalOffset)
            }

            var prefferedY = control.height - textItem.height
            return d.enoughRightOffset
                ? Qt.point(control.width + d.offset, prefferedY)
                : Qt.point(-d.minHorizontalOffset, prefferedY)
        }

        function getTopLeft()
        {
            return control.mapToItem(boundArea, 0, 0)
        }

        function getBottomRight()
        {
            return control.mapToItem(boundArea, control.width, control.height)
        }
    }
}
