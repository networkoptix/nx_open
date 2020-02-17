import QtQuick 2.0
import Nx.Controls 1.0

import "private"

Group
{
    id: control

    property string name: ""
    property alias caption: groupBox.title
    property string description: ""

    childrenItem: groupBox.contentItem

    width: parent.width
    implicitWidth: content.implicitWidth
    implicitHeight: content.implicitHeight

    contentItem: BottomPaddedItem
    {
        id: content

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
}
