// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Popups 1.0

PopupBase
{
    id: control

    required property string link

    title: qsTr("Open link?")
    messages: [qsTr("External link will open in the browser")]

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("Cancel")
            onClicked: control.close()
        },
        PopupButton
        {
            text: qsTr("Open")
            accented: true
            onClicked:
            {
                control.close()
                Qt.openUrlExternally(control.link)
            }
        }
    ]
}
