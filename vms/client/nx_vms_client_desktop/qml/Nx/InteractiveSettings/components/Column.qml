// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import "private"

Group
{
    id: control

    property string name: ""
    childrenItem: column

    width: parent.width

    contentItem: AlignedColumn
    {
        id: column

        labelWidthHint: (control.parent && control.parent.hasOwnProperty("labelWidth"))
            ? control.parent.labelWidth - control.x - column.x
            : 0
    }
}
