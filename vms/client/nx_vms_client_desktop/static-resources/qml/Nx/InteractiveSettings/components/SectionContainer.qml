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
    property alias contentEnabled: column.enabled
    property Item scrollBarParent: null
    property Item extraHeaderItem: null

    Scrollable
    {
        id: view

        verticalScrollBar: ScrollBar
        {
            parent: scrollBarParent ? scrollBarParent : view
            x: parent ? parent.width - width : 0
            height: parent ? parent.height : 0
            visible: view.visible
            opacity: enabled ? 1.0 : 0.3
            enabled: size < 0.9999
        }

        contentItem: Column
        {
            width:
            {
                return view.verticalScrollBar && view.verticalScrollBar.parent === view
                    ? Math.min(view.width, view.verticalScrollBar.x)
                    : view.width
            }

            Item
            {
                id: headerContainer

                width: parent.width
                height: extraHeaderItem ? extraHeaderItem.height : 0

                Binding
                {
                    target: extraHeaderItem
                    property: "width"
                    value: headerContainer.width
                }

                Binding
                {
                    target: extraHeaderItem
                    property: "parent"
                    value: headerContainer
                }
            }

            AlignedColumn
            {
                id: column
                width: parent.width
            }
        }
    }

    onCurrentIndexChanged:
    {
        if (currentIndex > 0 && children[currentIndex].hasOwnProperty("scrollBarParent"))
            children[currentIndex].scrollBarParent = scrollBarParent
    }
}
