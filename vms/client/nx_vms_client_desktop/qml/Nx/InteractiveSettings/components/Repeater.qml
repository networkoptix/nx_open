// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Controls 1.0

import "private"
import "private/utils.js" as Utils

Item
{
    property string addButtonCaption: ""

    readonly property Item childrenItem: column

    readonly property real buttonSpacing: 24

    implicitWidth: parent.width
    implicitHeight: column.implicitHeight + addButton.implicitHeight + buttonSpacing

    property int visibleItemsCount: 0

    property bool filled: false

    function processFilledChanged()
    {
        const lastFilled = lastFilledItemIndex()
        visibleItemsCount = Math.max(1, visibleItemsCount, lastFilled + 1)
        filled = (lastFilled >= 0)
    }

    AlignedColumn
    {
        id: column
        width: parent.width

        onChildrenChanged: initialVisibilityUpdateTimer.restart()
    }

    Button
    {
        id: addButton

        x: column.labelWidth + 8
        y: column.y + column.height + buttonSpacing
        text: addButtonCaption || qsTr("Add")

        visible: visibleItemsCount < column.children.length
        iconUrl: "qrc:///skin/buttons/plus.png"

        onClicked:
        {
            if (visibleItemsCount < column.children.length)
                ++visibleItemsCount
        }
    }

    Timer
    {
        // Queued update of visible items count.
        id: initialVisibilityUpdateTimer
        interval: 0
        running: false
        onTriggered:
        {
            visibleItemsCount = Math.max(1, lastFilledItemIndex() + 1)
            updateVisibility()
        }
    }

    onVisibleItemsCountChanged: updateVisibility()

    function lastFilledItemIndex()
    {
        for (var i = column.children.length - 1; i >= 0; --i)
        {
            if (Utils.isItemFilled(column.children[i]))
                return i
        }
        return -1
    }

    function updateVisibility()
    {
        for (var i = 0; i < column.children.length; ++i)
            column.children[i].visible = (i < visibleItemsCount)
    }
}
