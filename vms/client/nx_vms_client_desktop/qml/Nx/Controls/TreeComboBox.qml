// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core

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
