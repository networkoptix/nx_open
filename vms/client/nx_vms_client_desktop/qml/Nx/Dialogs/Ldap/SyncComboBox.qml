// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

ComboBox
{
    id: control

    property bool syncEnabled: true
    property var selectedSync: LdapSettings.Sync.usersAndGroups

    readonly property var kSync:
    [
        {"sync": LdapSettings.Sync.disabled, "text": qsTr("Never")},
        {"sync": LdapSettings.Sync.usersAndGroups, "text": qsTr("Always")},
        {"sync": LdapSettings.Sync.groupsOnly, "text": qsTr("On Log In")}
    ]

    readonly property var syncSubset: kSync.slice(syncEnabled ? 1 : 0)

    model: syncSubset.map(s => s.text)

    function indexToSync(index)
    {
        return syncSubset[index].sync
    }

    function syncToIndex(sync)
    {
        return syncSubset.findIndex(s => s.sync == sync)
    }

    onCurrentIndexChanged:
    {
        const newSync = indexToSync(currentIndex)
        if (selectedSync != newSync)
            selectedSync = newSync
    }

    Binding
    {
        target: control
        property: "currentIndex"
        value: control.syncToIndex(control.selectedSync)
    }
}
