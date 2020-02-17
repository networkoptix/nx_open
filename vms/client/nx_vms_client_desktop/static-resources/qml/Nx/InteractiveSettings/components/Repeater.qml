import QtQuick 2.0
import Nx.Controls 1.0

import "private"
import "private/utils.js" as Utils

Item
{
    property alias addButtonCaption: addButton.text

    readonly property Item childrenItem: column

    readonly property real buttonSpacing: 8

    implicitWidth: parent.width
    implicitHeight: column.implicitHeight + addButton.implicitHeight + buttonSpacing

    property int visibleItemsCount: 0

    function processFilledChanged()
    {
        if (!addButton.wasClicked)
            visibleItemsCount = lastFilledItemIndex() + 1
    }

    AlignedColumn
    {
        id: column
        width: parent.width

        onChildrenChanged:
        {
            visibleItemsCount = lastFilledItemIndex() + 1
            updateVisibility()
        }
    }

    Button
    {
        id: addButton

        property bool wasClicked: false

        anchors.bottom: parent.bottom
        x: column.labelWidth + 8
        text: qsTr("Add")

        visible: visibleItemsCount < column.children.length

        onClicked:
        {
            wasClicked = true

            if (visibleItemsCount < column.children.length)
                ++visibleItemsCount
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
