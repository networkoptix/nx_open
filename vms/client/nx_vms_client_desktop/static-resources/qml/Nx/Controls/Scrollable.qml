import QtQuick 2.0

import Nx.Controls 1.0

import "private"

// Since we want have consistent behavior we have to avoid flicking and manage scrolling by
// wheel directly with mouse area.
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
        readonly property real kThreeRowsScrollStep: 120 / flickable.contentHeight

        anchors.fill: parent
        onVerticalScrollBarChanged: updateScrollBar()
        Component.onCompleted: updateScrollBar()
        ScrollBar.vertical.stepSize: kThreeRowsScrollStep

        function updateScrollBar()
        {
            if (!verticalScrollBar)
                return

            ScrollBar.vertical = verticalScrollBar
            verticalScrollBar.stepSize = Qt.binding(function() { return scrollView.kThreeRowsScrollStep })
        }

        Flickable
        {
            id: flickable
            boundsBehavior: Flickable.StopAtBounds
            clip: true
        }
    }

    // TODO: Use Intruments in 4.2.
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

