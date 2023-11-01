// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Instruments

import nx.vms.client.core
import nx.vms.client.desktop

/**
 * A list view with not animated mouse wheel interaction and with hover tracking & highlighting.
 *
 * To control which items are hoverable the delegate must have "isSelectable" property.
 */
ListView
{
    id: listView

    property real itemHeightHint: 20
    property real scrollStepSize: itemHeightHint
    property color hoverHighlightColor: "grey"
    readonly property alias hoveredItem: hoverAndWheelHandler.hoveredItem
    readonly property alias scrollBar: scrollBar

    boundsBehavior: Flickable.StopAtBounds
    interactive: false
    currentIndex: -1

    Rectangle
    {
        id: hoverHighlight

        parent: listView.contentItem
        color: listView.hoverHighlightColor

        visible: hoveredItem && (!hoveredItem.hasOwnProperty("itemFlags")
            || (hoveredItem.itemFlags & Qt.ItemIsSelectable))

        Connections
        {
            target: listView

            function onHoveredItemChanged()
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

    AdaptiveMouseWheelTransmission { id: gearbox }

    HoverInstrument
    {
        id: hoverAndWheelHandler

        item: listView

        property Item hoveredItem: hovered && !WhatsThis.inWhatsThisMode()
            ? listView.itemAt(position.x + listView.contentX, position.y + listView.contentY)
            : null

        onMouseWheel: (wheel) =>
        {
            // TODO: imlement pixel scrolling for high precision touchpads.
            scrollBar.scrollBySteps(gearbox.transform(-wheel.angleDelta.y / 120.0))
        }
    }
}
