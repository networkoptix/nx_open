// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

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
    readonly property alias hoveredItem: mouseArea.hoveredItem
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

                const rect = listView.contentItem.mapFromItem(listView.hoveredItem,
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

        property Item hoveredItem: containsMouse && !WhatsThis.inWhatsThisMode()
            ? listView.itemAt(mouseX + listView.contentX, mouseY + listView.contentY)
            : null

        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        hoverEnabled: true
        z: -1

        AdaptiveMouseWheelTransmission { id: gearbox }

        onWheel: (wheel) =>
        {
            // TODO: imlement pixel scrolling for high precision touchpads.
            scrollBar.scrollBySteps(gearbox.transform(-wheel.angleDelta.y / 120.0))
        }
    }
}
