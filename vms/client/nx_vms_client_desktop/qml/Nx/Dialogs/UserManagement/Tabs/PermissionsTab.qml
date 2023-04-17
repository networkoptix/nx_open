// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    property bool enabled: true

    required property AccessSubjectEditingContext editingContext

    readonly property int columnCount: availableAccessRightDescriptors.length
    readonly property int kColumnWidth: 64

    property var buttonBox

    enum Mode
    {
        ViewMode,
        EditMode
    }

    property int currentMode: PermissionsTab.ViewMode

    readonly property bool editingEnabled: enabled && currentMode == PermissionsTab.EditMode

    readonly property var kAllNodes: [
        ResourceTree.NodeType.videoWalls,
        ResourceTree.NodeType.layouts,
        ResourceTree.NodeType.camerasAndDevices,
        ResourceTree.NodeType.integrations,
        ResourceTree.NodeType.webPages,
        ResourceTree.NodeType.servers]

    readonly property var availableAccessRightDescriptors:
    {
        if (control.editingEnabled || !control.editingContext)
            return AccessRightsList.items

        return Array.prototype.filter.call(AccessRightsList.items,
            item => (item.accessRight & control.editingContext.availableAccessRights) !== 0)
    }

    readonly property var availableAccessRights:
        Array.prototype.map.call(availableAccessRightDescriptors, item => item.accessRight)

    Item
    {
        id: accessRightsHeader

        anchors.right: control.right
        anchors.rightMargin: 8

        height: 64
        width: control.columnCount * control.kColumnWidth

        property var hoveredHeader: null

        property int hoveredAccessRight: hoveredHeader
            ? hoveredHeader.accessRight
            : tree.hoveredCell
                ? tree.hoveredCell.accessRight
                : 0

        Row
        {
            Repeater
            {
                model: control.availableAccessRightDescriptors

                AccessRightsHeaderItem
                {
                    id: accessRightItem

                    height: accessRightsHeader.height
                    width: control.kColumnWidth
                    icon: modelData.icon

                    color: accessRightsHeader.hoveredAccessRight == modelData.accessRight && enabled
                        ? ColorTheme.colors.light4
                        : ColorTheme.colors.light10

                    enabled: control.editingEnabled

                    GlobalToolTip.text: modelData.description

                    readonly property int index: Positioner.index
                    readonly property int accessRight: modelData.accessRight

                    onHoveredChanged:
                    {
                        if (hovered)
                            accessRightsHeader.hoveredHeader = accessRightItem
                        else if (accessRightsHeader.hoveredHeader == accessRightItem)
                            accessRightsHeader.hoveredHeader = null
                    }
                }
            }
        }
    }

    Rectangle
    {
        id: permissionsBackground

        anchors
        {
            left: accessRightsHeader.left
            leftMargin: -1
            right: tree.right
            rightMargin: -2
            top: tree.top
            topMargin: -1
            bottom: tree.bottom
            bottomMargin: -2
        }

        color: ColorTheme.colors.dark4
        border.color: ColorTheme.colors.dark8
    }

    ResourceSelectionTree
    {
        id: tree

        property var hoveredRow: null
        property var hoveredCell: hoveredRow ? hoveredRow.hoveredCell : null

        readonly property real scrollBarWidth: scrollBarVisible ? scrollBar.width : 0

        anchors.fill: parent
        anchors.topMargin: accessRightsHeader.height - 4
        anchors.rightMargin: 8 - scrollBarWidth
        anchors.leftMargin: 16

        expandsOnDoubleClick: false
        hoverHighlightColor: "transparent"
        showResourceStatus: false
        topMargin: 0

        resourceTypes: ResourceTree.ResourceFilter.camerasAndDevices
            | ResourceTree.ResourceFilter.integrations
            | ResourceTree.ResourceFilter.webPages
            | ResourceTree.ResourceFilter.layouts
            | ResourceTree.ResourceFilter.healthMonitors
            | ResourceTree.ResourceFilter.videoWalls

        filterText: searchField.text

        Binding
        {
            target: tree.model
            property: "resourceFilter"
            value: control.editingContext && control.editingContext.accessibleResourcesFilter
            when: control.editingContext && !control.editingEnabled
        }

        delegate: ResourceAccessDelegate
        {
            id: rowAccess

            editingContext: control.editingContext

            showExtraInfo: tree.model.extraInfoRequired
                || tree.model.isExtraInfoForced(resource)

            selectionMode: tree.selectionMode
            showResourceStatus: tree.showResourceStatus
            columnWidth: control.kColumnWidth
            rowHovered: tree.hoveredItem === this
            enabled: control.editingEnabled

            accessRightsList: control.availableAccessRights

            hoveredAccessRight: accessRightsHeader.hoveredHeader
                ? accessRightsHeader.hoveredAccessRight
                : 0

            onHoveredCellChanged:
            {
                if (hoveredCell)
                    tree.hoveredRow = rowAccess
                else if (tree.hoveredRow == rowAccess)
                    tree.hoveredRow = null
            }
        }
    }

    Item
    {
        anchors.left: parent.left
        anchors.top: accessRightsHeader.bottom
        anchors.bottom: parent.bottom

        width: searchField.width

        visible: tree.empty

        Placeholder
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: - (searchField.y + searchField.height)

            imageSource: "image://svg/skin/user_settings/no_resources.svg"

            text: qsTr("No resources")

            additionalText: currentMode == PermissionsTab.ViewMode
                ? qsTr("Try changing search criteria or enable editing to see available resources")
                : qsTr("Try changing search criteria")
        }
    }

    SearchField
    {
        id: searchField
        anchors.left: parent.left
        anchors.bottom: tree.top
        anchors.right: accessRightsHeader.left
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        anchors.rightMargin: 10
    }

    Item
    {
        // Covers dialog button box.

        anchors.top: parent.bottom

        height: control.buttonBox.height
        width: parent.width

        Switch
        {
            id: editingEnabledSwitch

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter

            visible: control.enabled

            text: checked ? qsTr("Editing enabled") : qsTr("Editing disabled")

            onCheckedChanged:
                control.currentMode = checked ? PermissionsTab.EditMode : PermissionsTab.ViewMode
        }
    }

    onCurrentModeChanged:
        editingEnabledSwitch.checked = currentMode == PermissionsTab.EditMode
}
