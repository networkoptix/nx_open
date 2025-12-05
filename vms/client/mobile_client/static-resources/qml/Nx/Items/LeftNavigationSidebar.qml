// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Models

import nx.vms.client.core

/**
 * Left sidebar navigation component with vertical menu items (Partners, Organizations, Sites).
 * When an organization is selected in the right panel, displays its tree structure with folders.
 */
Item
{
    id: leftSidebar

    implicitWidth: 330

    // Public properties
    property bool hasChannelPartners: false
    property bool hasOrganizations: false
    property int currentTab: OrganizationsModel.SitesTab
    property var organizationsModel: null
    property var selectedOrganizationIndex: null
    property var currentSelectedNodeId: null  // Currently selected node in the tree
    property string searchText: ""  // Search text for highlighting

    // Map of expanded nodes: { nodeId(string): true }
    property var expandedNodeIds: ({ })

    // Signals
    signal tabSelected(var selectedTabValue)
    signal organizationNodeClicked(var nodeId)

    // Restore expanded state after model reload.
    function restoreExpandedState()
    {
        if (!organizationsModel || !organizationTreeModel)
            return

        // Get all top-level rows from the source model.
        var rowCount = organizationsModel.rowCount()
        if (rowCount <= 0)
            return

        var topRows = []
        for (var r = 0; r < rowCount; ++r)
            topRows.push(r)

        organizationTreeModel.setExpanded(
            function(index) {
                const id = accessor.getData(index, "nodeId")
                return id && leftSidebar.expandedNodeIds[id] === true
            },
            topRows
        )
    }

    // Expand all parent nodes for a given node
    function expandParents(sortedIndex)
    {
        var parent = sortModel.parent(sortedIndex)
        while (parent !== NxGlobals.invalidModelIndex())
        {
            const parentId = accessor.getData(parent, "nodeId")
            if (parentId)
            {
                leftSidebar.expandedNodeIds[parentId] = true
                organizationTreeModel.setSourceExpanded(parent, true)
            }
            parent = sortModel.parent(parent)
        }
    }

    // Recursively search through tree and expand parents of matching nodes
    function searchAndExpandRecursive(parentIndex, searchLower)
    {
        if (!organizationsModel)
            return

        const rowCount = organizationsModel.rowCount(parentIndex)
        for (var i = 0; i < rowCount; i++)
        {
            const origIndex = organizationsModel.index(i, 0, parentIndex)
            const display = organizationsModel.data(origIndex, Qt.DisplayRole)
            const nodeType = organizationsModel.data(origIndex, OrganizationsModel.TypeRole)

            // Skip SitesNode and Systems
            if (nodeType === OrganizationsModel.SitesNode || nodeType === OrganizationsModel.System)
                continue

            // Check if this item matches the search
            var foundMatch = false
            if (display && display.toLowerCase().indexOf(searchLower) !== -1)
            {
                foundMatch = true
                // Map to sorted index and expand all parents
                const sortedIndex = sortModel.mapFromSource(origIndex)
                if (sortedIndex !== NxGlobals.invalidModelIndex())
                {
                    expandParents(sortedIndex)
                }
            }

            // Recursively search children
            if (organizationsModel.hasChildren(origIndex))
            {
                searchAndExpandRecursive(origIndex, searchLower)
            }
        }
    }

    // Expand all folders when searching to show full path to found items
    function expandAllForSearch()
    {
        if (!organizationsModel || !organizationTreeModel || !searchText || !selectedOrganizationIndex)
            return

        searchAndExpandRecursive(selectedOrganizationIndex, searchText.toLowerCase())
    }

    // Synchronize selection from external source (e.g., right panel).
    function selectNodeById(nodeId)
    {
        if (!nodeId || !organizationsModel)
            return

        leftSidebar.currentSelectedNodeId = nodeId

        // Get index from original model and map through sort model
        const origIndex = organizationsModel.indexFromNodeId(nodeId)
        if (origIndex === NxGlobals.invalidModelIndex())
            return

        const sortedIndex = sortModel.mapFromSource(origIndex)
        if (sortedIndex === NxGlobals.invalidModelIndex())
            return

        // Expand the node itself (if it's a folder or organization)
        const nodeType = accessor.getData(sortedIndex, "type")
        if (nodeType === OrganizationsModel.Folder || nodeType === OrganizationsModel.Organization)
        {
            leftSidebar.expandedNodeIds[nodeId] = true
            organizationTreeModel.setSourceExpanded(sortedIndex, true)
        }

        // Expand all parent nodes
        expandParents(sortedIndex)
    }

    // Handle search text changes
    onSearchTextChanged: {
        if (searchText) {
            expandAllForSearch()
        }
    }

    // Background
    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.colors.dark5
    }

    // Navigation list (visible when no organization is selected)
    NavigationListView
    {
        id: navigationList
        anchors.fill: parent
        visible: !selectedOrganizationIndex

        hasChannelPartners: leftSidebar.hasChannelPartners
        hasOrganizations: leftSidebar.hasOrganizations
        currentTab: leftSidebar.currentTab

        onTabSelected: function(tab) {
            leftSidebar.tabSelected(tab)
        }
    }

    // Organization tree view (visible when organization is selected)
    OrganizationTreeView
    {
        id: organizationTree
        anchors.fill: parent
        visible: selectedOrganizationIndex

        organizationsModel: leftSidebar.organizationsModel
        sortModel: sortModel
        organizationTreeModel: organizationTreeModel
        accessor: accessor
        currentSelectedNodeId: leftSidebar.currentSelectedNodeId
        searchText: leftSidebar.searchText

        onNodeClicked: function(nodeId) {
            const origIndex = leftSidebar.organizationsModel.indexFromNodeId(nodeId)
            if (origIndex === NxGlobals.invalidModelIndex())
                return

            const sortedIndex = sortModel.mapFromSource(origIndex)
            if (sortedIndex === NxGlobals.invalidModelIndex())
                return

            const nodeType = accessor.getData(sortedIndex, "type")

            // Always select and emit signal for organization, folder, or system
            if (nodeType === OrganizationsModel.Organization
                || nodeType === OrganizationsModel.Folder
                || nodeType === OrganizationsModel.System)
            {
                leftSidebar.currentSelectedNodeId = nodeId
                leftSidebar.organizationNodeClicked(nodeId)
            }
        }

        onExpandedChanged: function(nodeId, expanded) {
            if (expanded)
                leftSidebar.expandedNodeIds[nodeId] = true
            else
                delete leftSidebar.expandedNodeIds[nodeId]
        }
    }

    // Sort model for tree - sorts folders by name with natural sorting
    OrganizationsSortModel
    {
        id: sortModel
        sourceModel: leftSidebar.organizationsModel
    }

    // Model accessor for reading from sorted model
    ModelDataAccessor
    {
        id: accessor
        model: sortModel
    }

    // Linearization model for tree view
    LinearizationListModel
    {
        id: organizationTreeModel
        sourceModel: sortModel
        autoExpandAll: false

        Component.onCompleted: {
            if (leftSidebar.selectedOrganizationIndex) {
                const sortedIndex = sortModel.mapFromSource(leftSidebar.selectedOrganizationIndex)
                if (sortedIndex !== NxGlobals.invalidModelIndex()) {
                    organizationTreeModel.setSourceExpanded(sortedIndex, true)

                    const orgId = accessor.getData(sortedIndex, "nodeId")
                    if (orgId)
                        leftSidebar.expandedNodeIds[orgId] = true

                    leftSidebar.restoreExpandedState()
                }
            }
        }
    }

    onSelectedOrganizationIndexChanged: {
        if (selectedOrganizationIndex) {
            // Map through sort model
            const sortedIndex = sortModel.mapFromSource(selectedOrganizationIndex)
            if (sortedIndex !== NxGlobals.invalidModelIndex()) {
                // Expand the root organization.
                organizationTreeModel.setSourceExpanded(sortedIndex, true)

                const orgId = accessor.getData(sortedIndex, "nodeId")
                if (orgId)
                    leftSidebar.expandedNodeIds[orgId] = true

                // Try to restore previous state for all nodes.
                leftSidebar.restoreExpandedState()
            }
        }
    }

    // Restore state after full organization tree load.
    Connections
    {
        target: leftSidebar.organizationsModel

        function onFullTreeLoaded()
        {
            leftSidebar.restoreExpandedState()
        }
    }

    // And after linearization model reset (source reinitialized).
    Connections
    {
        target: organizationTreeModel

        function onModelReset()
        {
            leftSidebar.restoreExpandedState()
        }
    }

}
