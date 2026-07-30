// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

PopupBase
{
    id: popup

    icon: "image://skin/48x48/Solid/warning.svg?primary=yellow"
    title: qsTr("Disconnect")
    messages:
    [
        qsTr("Disconnect %1 from %2?", "%1 is a user, %2 is a system").arg(d.user).arg(d.system)
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

    QtObject
    {
        id: d

        readonly property string user: windowContext.sessionManager.isCloudSession
            ? cloudUserProfileWatcher.fullName
            : windowContext.mainSystemContext?.userWatcher.userFullName
        readonly property string system: windowContext.sessionManager.systemName
    }
}
