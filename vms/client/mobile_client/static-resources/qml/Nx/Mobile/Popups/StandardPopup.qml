// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

PopupBase
{
    id: control

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("OK")
            onClicked: control.close()
        }
    ]
}
