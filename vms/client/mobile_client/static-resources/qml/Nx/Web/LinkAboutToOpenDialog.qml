// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Popups 1.0

PopupBase
{
    id: control

    required property string link

    icon: "image://skin/48x48/Solid/warning.svg?primary=yellow"
    title: qsTr("Open external link in browser?")

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("Open")
            onClicked:
            {
                control.close()
                Qt.openUrlExternally(control.link)
            }
        },
        PopupButton
        {
            text: qsTr("Cancel")
            accented: true
            onClicked: control.close()
        }
    ]
}
