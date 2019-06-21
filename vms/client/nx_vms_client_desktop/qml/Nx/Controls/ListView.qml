import QtQuick 2.6

/**
 * A list view with not animated mouse wheel interaction and with hover tracking & highlighting.
 *
 * To control which items are hoverable/selectable, the delegate must have "selectable" property.
 *
 * Mouse click selection is not implemented at this level, the delegate must implement it.
 */
ListView
{
    id: listView

    property real scrollStepSize: 20 //< In pixels.
    property color selectionHighlightColor: "teal"
    property color hoverHighlightColor: "grey"
    readonly property Item hoveredItem: d.hoveredItem

    boundsBehavior: Flickable.StopAtBounds
    interactive: false
    currentIndex: -1
    clip: true

    highlightMoveDuration: 0
    highlightMoveVelocity: -1
    highlightResizeDuration: 0
    highlightResizeVelocity: -1

    highlight: Rectangle
    {
        color: selectionHighlightColor
    }

    Rectangle
    {
        id: hoverHighlight

        parent: listView.contentItem
        color: listView.hoverHighlightColor

        visible: d.hoveredItem
            && (!d.hoveredItem.hasOwnProperty("selectable") || d.hoveredItem.selectable)
            && d.hoveredItem !== listView.currentItem

        Connections
        {
            target: d

            onHoveredItemChanged:
            {
                if (!d.hoveredItem)
                    return

                var rect = listView.contentItem.mapFromItem(d.hoveredItem,
                    0, 0, d.hoveredItem.width, d.hoveredItem.height)

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
        stepSize: scrollStepSize / listView.height
    }

    MouseArea
    {
        acceptedButtons: Qt.LeftButton
        parent: listView.contentItem
        anchors.fill: parent
        hoverEnabled: true

        onWheel:
        {
            if (wheel.angleDelta.y > 0)
                scrollBar.decrease()
            else
                scrollBar.increase()
        }

        onPositionChanged:
            d.hoveredItem = listView.itemAt(mouse.x, mouse.y)

        onExited:
            d.hoveredItem = null
    }

    QtObject
    {
        id: d
        property Item hoveredItem: null
    }
}
