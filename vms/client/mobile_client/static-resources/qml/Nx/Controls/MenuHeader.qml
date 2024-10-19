// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

MenuItem
{
    id: control

    font.bold: true

    MenuSeparator
    {
        y: parent.height - height
        width: parent.width
        parent: control
    }
}
