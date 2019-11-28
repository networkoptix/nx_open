import QtQuick 2.0
import QtQuick.Layouts 1.10

import Nx.Controls 1.0

import "private"

StackLayout
{
    id: control

    property string name: ""
    property Item childrenItem: column
    property StackLayout sectionsItem: control
    property bool contentEnabled: column.enabled
    property Item scrollBarParent: null

    Scrollable
    {
        id: view

        verticalScrollBar: ScrollBar
        {
            parent: scrollBarParent ? scrollBarParent : view
            x: parent ? parent.width - width : 0
            height: parent ? parent.height : 0
            visible: view.visible
        }

        contentItem: AlignedColumn
        {
            id: column

            width:
            {
                return view.verticalScrollBar && view.verticalScrollBar.parent === view
                    ? Math.min(view.width, view.verticalScrollBar.x)
                    : view.width
            }
        }
    }

    onCurrentIndexChanged:
    {
        if (currentIndex > 0 && children[currentIndex].hasOwnProperty("scrollBarParent"))
            children[currentIndex].scrollBarParent = scrollBarParent
    }
}

