// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Mobile.Popups 1.0

import nx.vms.client.core

PopupBase
{
    id: control

    buttonBoxButtons:
    [
        PopupButton
        {
            text: qsTr("OK")
            onClicked: control.close()
        },
        PopupButton
        {
            text: qsTr("Open account settings")
            accented: true
            onClicked:
            {
                Qt.openUrlExternally(CloudUrlHelper.accountSecurityUrl())
                control.close()
            }
        }
    ]
}
