import QtQuick 2.6

/**
 * A list view with not animated mouse wheel interaction and with hover tracking & highlighting.
 *
 * To control which items are hoverable the delegate must have "isSelectable" property.
 */
ListView
{
    id: listView

    property real scrollStepSize: 20 //< In pixels.
    property color hoverHighlightColor: "grey"
    readonly property Item hoveredItem: mouseArea.hoveredItem

    boundsBehavior: Flickable.StopAtBounds
    interactive: false
    currentIndex: -1

    Rectangle
    {
        id: hoverHighlight

        parent: listView.contentItem
        color: listView.hoverHighlightColor

        visible: hoveredItem
            && (!hoveredItem.hasOwnProperty("isSelectable") || hoveredItem.isSelectable)

        Connections
        {
            target: listView

            onHoveredItemChanged:
            {
                if (!hoveredItem)
                    return

                var rect = listView.contentItem.mapFromItem(listView.hoveredItem,
                    0, 0, listView.hoveredItem.width, listView.hoveredItem.height)

                hoverHighlight.x = rect.x
                hoverHighlight.y = rect.y
                hoverHighlight.width = rect.width
                hoverHighlight.height = rect.height
            }
        }
    }

    ScrollBar.vertical: ScrollBar
    {
        id: scrollBar
        stepSize: scrollStepSize / listView.contentHeight
    }

    MouseArea
    {
        id: mouseArea

        property Item hoveredItem: null

        acceptedButtons: Qt.LeftButton
        anchors.fill: parent
        hoverEnabled: true
        z: -1

        onWheel:
        {
            if (wheel.angleDelta.y > 0)
                scrollBar.decrease()
            else
                scrollBar.increase()
        }

        onPositionChanged:
            hoveredItem = listView.itemAt(mouse.x, mouse.y)

        onExited:
            hoveredItem = null
    }
}
