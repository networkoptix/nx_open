// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Nx.Core
import Nx.Items
import Nx.Mobile

import nx.vms.client.mobile

Pane
{
    id: systemInformationPanel

    implicitWidth: parent ? parent.width : contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: 0

    background: null

    QnCloudSystemInformationWatcher
    {
        id: cloudInformationWatcher
    }

    contentItem: SystemInformationBlock
    {
        topPadding: 16
        bottomPadding: 0
        systemName: sessionManager.systemName
        address: sessionManager.sessionHost
        user: currentUserWatcher.userName
        ownerDescription: cloudInformationWatcher.ownerDescription
        cloud: sessionManager.isCloudSession
    }
}
