// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Popups 1.0

SslCertificateDialogBase
{
    id: control

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("Connect Anyway")
            onClicked:
            {
                control.result = "connect"
                control.close()
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
