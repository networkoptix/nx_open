// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

import "../Components"

Item
{
    id: control

    required property AccessSubjectEditingContext editingContext

    readonly property int columnCount: AccessRightsList.items.length
    readonly property int kColumnWidth: 48

    property var buttonBox

    readonly property var kAllNodes: [
        ResourceTree.NodeType.videoWalls,
        ResourceTree.NodeType.layouts,
        ResourceTree.NodeType.camerasAndDevices,
        ResourceTree.NodeType.webPages,
        ResourceTree.NodeType.servers]

    Item
    {
        id: accessRightsHeader

        anchors.right: tree.right

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
                model: AccessRightsList.items

                AccessRightsHeaderItem
                {
                    id: accessRightItem

                    height: accessRightsHeader.height
                    width: control.kColumnWidth
                    icon: modelData.icon

                    color: accessRightsHeader.hoveredAccessRight == modelData.accessRight && enabled
                        ? ColorTheme.colors.light4
                        : ColorTheme.colors.light10

                    enabled: !allPermissionsAndResourcesSwitch.checked

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

                    onClicked:
                    {
                        const prev = control.editingContext.hasOwnAccessRight(
                            null, modelData.accessRight)
                        control.editingContext.setOwnAccessRight(null, modelData.accessRight, !prev)
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

        anchors.fill: parent
        anchors.topMargin: accessRightsHeader.height
        anchors.rightMargin: 8
        anchors.leftMargin: 16

        enabled: !allPermissionsAndResourcesSwitch.checked
        collapsedNodes: allPermissionsAndResourcesSwitch.checked ? control.kAllNodes : []

        hoverHighlightColor: "transparent"
        showResourceStatus: false
        topMargin: 0

        resourceTypes: ResourceTree.ResourceFilter.camerasAndDevices
            | ResourceTree.ResourceFilter.webPages
            | ResourceTree.ResourceFilter.layouts
            | ResourceTree.ResourceFilter.healthMonitors
            | ResourceTree.ResourceFilter.videoWalls

        filterText: allPermissionsAndResourcesSwitch.checked ? "" : searchField.text

        delegate: ResourceAccessDelegate
        {
            id: rowAccess

            editingContext: control.editingContext
            showExtraInfo: tree.model.extraInfoRequired
                || tree.model.isExtraInfoForced(resource)
            selectionMode: tree.selectionMode
            showResourceStatus: tree.showResourceStatus
            columnWidth: control.kColumnWidth
            hoveredAccessRight: accessRightsHeader.hoveredHeader
                ? accessRightsHeader.hoveredAccessRight
                : 0
            rowHovered: tree.hoveredItem === this
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
            additionalText:
                qsTr("Try changing search criteria or enable showing all permissions and resources")
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

        SwitchWithText
        {
            id: allPermissionsAndResourcesSwitch

            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter

            text: qsTr("All permissions & resources")
            color: ColorTheme.colors.light10

            checked: Array.prototype.every.call(AccessRightsList.items,
                item => control.editingContext.hasOwnAccessRight(
                    NxGlobals.uuid(""), item.accessRight))

            onClicked:
            {
                const newValue = !checked
                Array.prototype.map.call(AccessRightsList.items,
                    item => control.editingContext.setOwnAccessRight(
                        NxGlobals.uuid(""), item.accessRight, newValue))

                tree.collapsedNodes = checked ? control.kAllNodes : []
            }
        }

        Connections
        {
            target: control.editingContext

            function onResourceAccessChanged()
            {
                allPermissionsAndResourcesSwitch.checked =
                    Array.prototype.every.call(AccessRightsList.items,
                        item => control.editingContext.hasOwnAccessRight(
                            NxGlobals.uuid(""), item.accessRight))
            }
        }
    }
}
