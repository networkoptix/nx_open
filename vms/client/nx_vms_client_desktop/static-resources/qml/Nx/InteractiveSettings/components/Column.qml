import QtQuick 2.0

import "private"

Group
{
    id: control

    property string name: ""
    childrenItem: column

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    contentItem: AlignedColumn
    {
        id: column

        labelWidthHint: (control.parent && control.parent.hasOwnProperty("labelWidth"))
            ? control.parent.labelWidth - control.x - column.x
            : 0
    }
}
