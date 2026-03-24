// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Models

/**
 * Organization tree view component showing hierarchical structure of folders.
 */
Item
{
    id: treeView

    property var organizationsModel
    property var sortModel
    property var organizationTreeModel
    property var accessor
    property var currentSelectedNodeId
    property string searchText: ""

    property var sortedOrganizationIndex: null
    readonly property bool hasOrganization: sortedOrganizationIndex != null

    // Tracks whether the root organization node is expanded in the sidebar.
    property bool orgExpanded: true

    onSortedOrganizationIndexChanged:
    {
        if (sortedOrganizationIndex != null)
            orgExpanded = true
    }

    signal nodeClicked(var nodeId)
    signal expandedChanged(var nodeId, bool expanded)

    // Search highlighting support
    property var currentSearchRegExp: null
    readonly property color highlightColor: ColorTheme.colors.yellow_d1

    onSearchTextChanged:
    {
        treeView.currentSearchRegExp = searchText
            ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(searchText)})`, 'i')
            : null
    }

    function highlightMatchingText(text)
    {
        if (currentSearchRegExp)
            return NxGlobals.highlightMatch(text, currentSearchRegExp, highlightColor)

        return NxGlobals.toHtmlEscaped(text)
    }

    // Organization root item shown as the collapsible tree header.
    TreeNodeItem
    {
        id: orgHeaderItem

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        visible: treeView.hasOrganization

        nodeData: treeView.hasOrganization
            ? ({
                nodeId: treeView.accessor.getData(treeView.sortedOrganizationIndex, "nodeId"),
                display: treeView.accessor.getData(treeView.sortedOrganizationIndex, "display"),
                type: OrganizationsModel.Organization,
                hasChildren: true,
                expanded: treeView.orgExpanded,
                level: 0
            })
            : null
        isSelected: nodeData && nodeData.nodeId === treeView.currentSelectedNodeId

        organizationsModel: treeView.organizationsModel
        sortModel: treeView.sortModel
        organizationTreeModel: treeView.organizationTreeModel
        accessor: treeView.accessor

        onNodeClicked: function(nodeId) { treeView.nodeClicked(nodeId) }
        onExpandedToggled: function(nodeId, expanded) { treeView.orgExpanded = expanded }
    }

    ScrollView
    {
        anchors.top: treeView.hasOrganization ? orgHeaderItem.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: treeView.hasOrganization ? 4 : 20
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 20
        clip: true

        visible: !treeView.hasOrganization || treeView.orgExpanded

        ListView
        {
            id: treeListView

            model: treeView.organizationTreeModel
            // Spacing is handled by the wrapper Item below so that hidden nodes
            // (System, SitesNode) do not contribute any gap.
            spacing: 0

            delegate: Item
            {
                id: delegateWrapper

                width: treeListView.width
                // Visible nodes get 4 px bottom gap; hidden nodes take no space at all.
                height: nodeItem.visible ? nodeItem.implicitHeight + 4 : 0

                TreeNodeItem
                {
                    id: nodeItem

                    width: parent.width
                    nodeData: model
                    // Shift level by 1 so folders appear indented under the org header.
                    nodeLevel: (nodeData ? (nodeData.level || 0) : 0) + 1
                    isSelected: nodeData && nodeData.nodeId === treeView.currentSelectedNodeId

                    organizationsModel: treeView.organizationsModel
                    sortModel: treeView.sortModel
                    organizationTreeModel: treeView.organizationTreeModel
                    accessor: treeView.accessor

                    // Hide SitesNode and Systems in the tree
                    visible: {
                        if (!nodeData)
                            return true

                        const nodeType = nodeData.type

                        if (nodeType === OrganizationsModel.SitesNode)
                            return false

                        if (nodeType === OrganizationsModel.System)
                            return false

                        return true
                    }

                    onNodeClicked: function(nodeId) {
                        treeView.nodeClicked(nodeId)
                    }

                    onExpandedToggled: function(nodeId, expanded) {
                        treeView.expandedChanged(nodeId, expanded)
                    }
                }
            }
        }
    }

    // Tree node item component
    component TreeNodeItem: AbstractButton
    {
        id: treeNode

        property var nodeData
        property int nodeType: nodeData?.type ?? OrganizationsModel.None
        property int nodeLevel: nodeData?.level ?? 0
        property bool hasChildren: nodeData?.hasChildren ?? false
        property bool isExpanded: nodeData?.expanded ?? false
        property bool isSelected: false

        property var organizationsModel
        property var sortModel
        property var organizationTreeModel
        property var accessor

        signal nodeClicked(var nodeId)
        signal expandedToggled(var nodeId, bool expanded)

        implicitHeight: 40
        focusPolicy: Qt.StrongFocus

        background: Rectangle
        {
            color: treeNode.isSelected
                ? ColorTheme.colors.dark10
                : (treeNode.hovered ? ColorTheme.colors.dark8 : "transparent")
            radius: 4
        }

        contentItem: Item
        {
            RowLayout
            {
                anchors.fill: parent
                spacing: 0
                anchors.leftMargin: 4 + (treeNode.nodeLevel * 20)
                anchors.rightMargin: 4

                // Expand/collapse arrow for folders and organization (40x40 with 10px buffer on all sides)
                Item
                {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    visible: treeNode.hasChildren &&
                        (treeNode.nodeType === OrganizationsModel.Folder ||
                         treeNode.nodeType === OrganizationsModel.Organization)

                    MouseArea
                    {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            mouse.accepted = true

                            // Get index from original model
                            const origIndex = treeNode.organizationsModel.indexFromNodeId(treeNode.nodeData.nodeId)
                            if (origIndex === NxGlobals.invalidModelIndex())
                                return

                            // Map through sort model
                            const sortedIndex = treeNode.sortModel.mapFromSource(origIndex)
                            if (sortedIndex === NxGlobals.invalidModelIndex())
                                return

                            const currentlyExpanded = treeNode.organizationTreeModel.isSourceExpanded(sortedIndex)
                            const nowExpanded = !currentlyExpanded

                            treeNode.organizationTreeModel.setSourceExpanded(sortedIndex, nowExpanded)

                            // Emit signal to update expanded state
                            treeNode.expandedToggled(treeNode.nodeData.nodeId, nowExpanded)
                        }
                    }

                    ColoredImage
                    {
                        anchors.centerIn: parent
                        sourceSize: Qt.size(20, 20)
                        sourcePath: treeNode.isExpanded
                            ? "image://skin/20x20/Solid/arrow_open.svg"
                            : "image://skin/20x20/Solid/arrow_right.svg"
                        primaryColor: ColorTheme.colors.light17
                    }
                }

                // Spacer when no arrow
                Item
                {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    visible: !treeNode.hasChildren ||
                        (treeNode.nodeType !== OrganizationsModel.Folder &&
                         treeNode.nodeType !== OrganizationsModel.Organization)
                }

                // Icon
                ColoredImage
                {
                    Layout.alignment: Qt.AlignVCenter
                    sourceSize: Qt.size(20, 20)
                    visible: treeNode.nodeType !== OrganizationsModel.None

                    primaryColor: treeNode.isSelected ? ColorTheme.colors.light4 : ColorTheme.colors.light10

                    sourcePath: {
                        switch (treeNode.nodeType)
                        {
                            case OrganizationsModel.Organization:
                                return "image://skin/20x20/Solid/organization.svg"
                            case OrganizationsModel.Folder:
                                return treeNode.isExpanded
                                    ? "image://skin/20x20/Solid/folder_open.svg"
                                    : "image://skin/20x20/Solid/folder_close.svg"
                            case OrganizationsModel.System:
                                return "image://skin/20x20/Solid/site_local.svg"
                            default:
                                return ""
                        }
                    }
                }

                // Text with search highlighting
                Text
                {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: 4
                    text: {
                        const displayText = treeNode.nodeData ? (treeNode.nodeData.display || "") : ""
                        // Use the parent's highlightMatchingText function
                        return treeView.highlightMatchingText(displayText)
                    }
                    textFormat: Text.StyledText
                    color: treeNode.isSelected ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                    font.pixelSize: 14
                    font.weight: Font.Normal
                    elide: Text.ElideRight
                }
            }
        }

        onClicked: treeNode.nodeClicked(treeNode.nodeData ? treeNode.nodeData.nodeId : null)
    }
}
