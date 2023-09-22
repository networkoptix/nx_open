// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    property int pixelsPerLine: 20 // Default value in Qt, originally comes from QTextEdit.

    // Allow custom content resize when vertical ScrollBar appears.
    property int scrollBarWidth: scrollView.ScrollBar.vertical.visible
        ? scrollView.ScrollBar.vertical.width
        : 0

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

            contentWidth: control.contentItem ? control.contentItem.width : 0
            contentHeight: control.contentItem ? control.contentItem.height : 0

            function scrollContentY(pixelDelta)
            {
                if (!pixelDelta)
                    return

                const minY = flickable.originY - flickable.topMargin
                const maxY = (flickable.originY + flickable.bottomMargin
                    + flickable.contentHeight) - flickable.height


                const newContentY = Math.max(minY, Math.min(maxY, flickable.contentY - pixelDelta))
                const accepted = flickable.contentY != newContentY
                flickable.contentY = newContentY

                return accepted
            }
        }
    }

    // TODO: Use Intruments in 4.2.
    MouseArea
    {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton

        AdaptiveMouseWheelTransmission { id: gearbox }

        onWheel: (wheel) =>
        {
            wheel.accepted = flickable.contentHeight > flickable.height
                && flickable.scrollContentY(gearbox.transform(getPixelDelta(wheel)))
        }

        function getPixelDelta(wheel)
        {
            // Standard mouse values.
            const kUnitsPerDegree = 8
            const kDegreesPerStep = 15.0

            const degrees = wheel.angleDelta.y / kUnitsPerDegree

            if (degrees)
                return Qt.styleHints.wheelScrollLines * pixelsPerLine * (degrees / kDegreesPerStep)

            return wheel.pixelDelta.y
        }
    }
}
