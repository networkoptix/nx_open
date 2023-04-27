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

    readonly property bool editingEnabled: enabled

    readonly property var availableAccessRightDescriptors:
        Array.prototype.slice.call(AccessRightsList.items) //< Deep copy to JS for optimization.

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

        resourceTypes:
        {
            const kAllTypes = ResourceTree.ResourceFilter.camerasAndDevices
                | ResourceTree.ResourceFilter.integrations
                | ResourceTree.ResourceFilter.webPages
                | ResourceTree.ResourceFilter.layouts
                | ResourceTree.ResourceFilter.healthMonitors
                | ResourceTree.ResourceFilter.videoWalls

            return filterButton.resourceTypeFilter || kAllTypes
        }

        topLevelNodesPolicy: ResourceSelectionModel.TopLevelNodesPolicy.showAlways

        filterText: searchField.text

        Binding
        {
            target: tree.model
            property: "resourceFilter"
            value: control.editingContext && control.editingContext.accessibleResourcesFilter
            when: control.editingContext && filterButton.withPermissionsOnly
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
            anchors.verticalCenterOffset: -(searchField.y + searchField.height)

            imageSource: "image://svg/skin/user_settings/no_resources.svg"

            text: qsTr("No resources")
            additionalText: qsTr("Try changing search criteria")
        }
    }

    SearchField
    {
        id: searchField

        anchors.left: parent.left
        anchors.bottom: tree.top
        anchors.right: filterButton.left
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        anchors.rightMargin: 4
    }

    ResourceFilterButton
    {
        id: filterButton

        anchors.top: searchField.top
        anchors.bottom: searchField.bottom
        anchors.right: accessRightsHeader.left
        anchors.rightMargin: 16
        width: 32

        onWithPermissionsOnlyChanged:
            editingContext.resetAccessibleResourcesFilter()
    }
}
