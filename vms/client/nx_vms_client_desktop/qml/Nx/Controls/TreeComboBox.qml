// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0

import nx.vms.client.desktop 1.0

ComboBox
{
    id: control

    property alias treeModel: linearizationListModel.sourceModel

    tabRole: "level"

    model: LinearizationListModel
    {
        id: linearizationListModel
        autoExpandRoleName: control.textRole || "display"
    }
}
