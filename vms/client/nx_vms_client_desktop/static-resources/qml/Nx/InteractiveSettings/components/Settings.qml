import QtQuick 2.0

import Nx.Controls 1.0

import "private"

Scrollable
{
    id: control

    property string name: ""
    property Item childrenItem: contentItem
    property bool contentEnabled: column.enabled

    contentItem: AlignedColumn
    {
        id: column

        width:
        {
            var scrollBar = control.scrollView.ScrollBar.vertical
            return scrollBar && scrollBar.parent === control.scrollView
                ? Math.min(control.scrollView.width, scrollBar.x)
                : control.scrollView.width
        }
    }
}

