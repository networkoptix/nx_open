import QtQuick 2.0

import Nx.Controls 1.0

import "private"

// Since we want to avoid flicking behaviour we have to manage scrolling by wheel directly
// with mouse area.
Item
{
    id: control

    property alias verticalScrollBar: scrollView.verticalScrollBar
    property alias scrollView: scrollView
    property alias background: scrollView.background
    property Item contentItem

    onContentItemChanged:
    {
        if (contentItem)
            contentItem.parent = flickable.contentItem
    }

    function updateScrollBar()
    {
        scrollView.updateScrollBar()
    }

    ScrollView
    {
        id: scrollView

        property ScrollBar verticalScrollBar: null

        anchors.fill: parent
        onVerticalScrollBarChanged: updateScrollBar()
        Component.onCompleted: updateScrollBar()

        function updateScrollBar()
        {
            if (verticalScrollBar)
                ScrollBar.vertical = verticalScrollBar
        }

        Flickable
        {
            id: flickable
            boundsBehavior: Flickable.StopAtBounds
            clip: true
        }
    }

    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel:
        {
            if (!wheel.angleDelta.y)
                return

            if (wheel.angleDelta.y > 0)
                scrollView.ScrollBar.vertical.decrease()
            else
                scrollView.ScrollBar.vertical.increase()
        }
    }
}

