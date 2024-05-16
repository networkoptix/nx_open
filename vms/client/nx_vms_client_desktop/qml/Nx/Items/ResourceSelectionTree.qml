// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Common 1.0
import Nx.Controls 1.0
import Nx.Layout 1.0
import Nx.Models 1.0
import Nx.Utils 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

import "private"

TreeView
{
    id: resourceSelectionTree

    readonly property real kRowHeight: 20
    readonly property real kSeparatorRowHeight: 16

    property bool showResourceStatus: true

    property int resourceTypes: 0
    property int selectionMode: { return ResourceTree.ResourceSelection.multiple }
    property int topLevelNodesPolicy:
        { return ResourceSelectionModel.TopLevelNodesPolicy.hideEmpty }
    property bool webPagesAndIntegrationsCombined: false
    property string filterText

    topMargin: 8
    leftMargin: 0
    indentation: 20
    scrollStepSize: kRowHeight
    hoverHighlightColor: ColorTheme.transparent(ColorTheme.colors.dark8, 0.4)
    selectionHighlightColor: ColorTheme.transparent(ColorTheme.colors.brand_core, 0.3)
    autoExpandRoleName: "autoExpand"
    expandsOnDoubleClick: true
    rootIsDecorated: true
    selectionEnabled: false
    editable: false

    model: ResourceSelectionModel
    {
        id: resourceSelectionModel

        context: systemContext
        resourceTypes: resourceSelectionTree.resourceTypes
        selectionMode: resourceSelectionTree.selectionMode
        topLevelNodesPolicy: resourceSelectionTree.topLevelNodesPolicy
        webPagesAndIntegrationsCombined: resourceSelectionTree.webPagesAndIntegrationsCombined
        filterText: resourceSelectionTree.filterText
    }

    delegate: Component
    {
        ResourceSelectionDelegate
        {
            showResourceStatus: resourceSelectionTree.model.showResourceStatus || false
            showExtraInfo: resourceSelectionModel.extraInfoRequired
                || resourceSelectionModel.isExtraInfoForced(resource)
            selectionMode: resourceSelectionTree.selectionMode
            implicitHeight: isSeparator ? kSeparatorRowHeight : kRowHeight
        }
    }
}
