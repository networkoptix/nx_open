import QtQuick 2.0
import Nx.Controls 1.0

import "private"

BottomPaddedItem
{
    id: control

    property string name: ""
    property alias caption: groupBox.title
    property string description: ""

    property alias childrenItem: groupBox.contentItem

    isGroup: true
    width: parent.width

    paddedItem: GroupBox
    {
        id: groupBox

        contentItem: AlignedColumn
        {
            id: column

            Binding
            {
                target: column
                property: "labelWidthHint"
                value: control.parent.labelWidth - control.x - column.x
                when: control.parent && control.parent.hasOwnProperty("labelWidth")
            }
        }
    }
}
