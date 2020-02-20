import QtQuick 2.0
import Nx.Controls 1.0

import "private"
import "private/utils.js" as Utils

Item
{
    property string addButtonCaption: ""

    readonly property Item childrenItem: column

    readonly property real buttonSpacing: 8

    implicitWidth: parent.width
    implicitHeight: column.implicitHeight + addButton.implicitHeight + buttonSpacing

    property int visibleItemsCount: 0

    function processFilledChanged()
    {
        visibleItemsCount = Math.max(visibleItemsCount, lastFilledItemIndex() + 1)
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

        anchors.bottom: parent.bottom
        x: column.labelWidth + 8
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
            visibleItemsCount = lastFilledItemIndex() + 1
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
        return 0
    }

    function updateVisibility()
    {
        for (var i = 0; i < column.children.length; ++i)
            column.children[i].visible = (i < visibleItemsCount)
    }
}
