// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Dialogs

StandardDialog
{
    id: control

    required property string link

    title: qsTr("Open external link in browser?")
    buttonsModel: [{"id": qsTr("Open")}, {"id": "CANCEL", "accented": true}]

    onButtonClicked: (buttonId) =>
    {
        if (buttonId === "CANCEL")
            return

        Qt.openUrlExternally(control.link)
    }
}
