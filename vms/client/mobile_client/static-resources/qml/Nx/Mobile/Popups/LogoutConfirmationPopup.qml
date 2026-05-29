// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

PopupBase
{
    id: popup

    icon: "image://skin/48x48/Solid/warning.svg?primary=yellow"
    title: qsTr("Disconnect")
    messages:
    [
        qsTr("Are you sure you want to disconnect from %1?", "%1 is a system name")
            .arg(windowContext.sessionManager.systemName)
    ]
    messagesTextAlignment: Text.AlignHCenter

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("Cancel")

            onClicked: popup.close()
        },
        PopupButton
        {
            text: qsTr("Disconnect")
            accented: true

            onClicked:
            {
                popup.close()
                windowContext.sessionManager.stopSessionByUser()
            }
        }
    ]
}
