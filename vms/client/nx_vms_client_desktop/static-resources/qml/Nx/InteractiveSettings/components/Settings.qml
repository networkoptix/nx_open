import QtQuick 2.0

import Nx.Controls 1.0

import "private"

ScrollView
{
    id: scrollView

    property string name: ""
    property Item childrenItem: column
    property alias contentEnabled: column.enabled
    property ScrollBar verticalScrollBar: null

    onVerticalScrollBarChanged: updateScrollBar()
    Component.onCompleted: updateScrollBar()

    function updateScrollBar()
    {
        if (verticalScrollBar)
            ScrollBar.vertical = verticalScrollBar
    }

    Flickable
    {
        boundsBehavior: Flickable.StopAtBounds

        clip: true

        AlignedColumn
        {
            id: column
            width: 
            {
                if (scrollView.ScrollBar.vertical && scrollView.ScrollBar.vertical.parent === scrollView)
                    return Math.min(scrollView.width, scrollView.ScrollBar.vertical.x)

                return scrollView.width
            }
        }
    }
}
