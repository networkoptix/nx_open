// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx
import Nx.Core
import Nx.Controls
import Nx.Instruments
import Nx.Items

import nx.client.core
import nx.vms.client.core
import nx.vms.client.desktop

import "../Components"

Item
{
    id: control

    required property AccessSubjectEditingContext editingContext

    readonly property int columnCount: availableAccessRightDescriptors.length

    readonly property int kRowHeight: 28
    readonly property int kMinColumnWidth: 64

    property var buttonBox

    property bool editingEnabled: true

    property bool advancedMode: true

    readonly property var availableAccessRightDescriptors:
        Array.prototype.slice.call(AccessRightsList.items) //< Deep copy to JS for optimization.

    readonly property var availableAccessRights:
        Array.prototype.map.call(availableAccessRightDescriptors, item => item.accessRight)

    enum Mode
    {
        SimpleMode,
        AdvancedMode
    }
    property int mode: PermissionsTab.SimpleMode

    Item
    {
        id: accessRightsHeader

        property int columnWidth: Math.max(control.kMinColumnWidth,
             Math.round((control.width * 0.5) / control.columnCount))

        anchors.right: control.right
        anchors.rightMargin: 9

        height: 64
        width: control.columnCount * columnWidth

        property int hoveredAccessRight: 0
        property var hoverPos

        Row
        {
            spacing: 1
            leftPadding: 1

            Repeater
            {
                id: accessRightsButtonsRepeater

                model: control.availableAccessRightDescriptors

                AccessRightsHeaderItem
                {
                    id: accessRightItem

                    readonly property int accessRight: modelData.accessRight

                    height: accessRightsHeader.height
                    width: accessRightsHeader.columnWidth - 1

                    icon: modelData.icon

                    color: tree.hoveredAccessRight == modelData.accessRight && enabled
                        ? ColorTheme.colors.light4
                        : ColorTheme.colors.light10

                    enabled: control.editingEnabled
                    interactive: !tree.selectionEmpty

                    GlobalToolTip.text: modelData.description

                    onHoveredChanged:
                    {
                        if (hovered)
                        {
                            accessRightsHeader.hoveredAccessRight = accessRight
                            accessRightsHeader.hoverPos = x
                        }
                        else if (accessRightsHeader.hoveredAccessRight == accessRight)
                        {
                            accessRightsHeader.hoveredAccessRight = 0
                            accessRightsHeader.hoverPos = undefined
                        }
                    }

                    onClicked:
                    {
                        if (!control.editingContext)
                            return

                        control.editingContext.modifyAccessRights(tree.selection(),
                            accessRight,
                            tree.nextBatchCheckState === Qt.Checked,
                            automaticDependenciesSwitch.checked)
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
            right: tree.right
            top: tree.top
            bottom: tree.bottom
        }

        color: control.editingEnabled && control.mode === PermissionsTab.AdvancedMode
            ? ColorTheme.colors.dark5
            : ColorTheme.colors.dark7

        Rectangle
        {
            id: hoveredColumnBackground

            x: accessRightsHeader.hoverPos || 0
            width: accessRightsHeader.columnWidth - 1
            height: permissionsBackground.height

            visible: accessRightsHeader.hoverPos !== undefined
            color: tree.tableHoverColor
        }
    }

    ResourceSelectionTree
    {
        id: tree

        property var hoveredRow: null
        readonly property var hoveredCell: hoveredRow ? hoveredRow.hoveredCell : null
        readonly property int hoveredAccessRight: hoveredCell ? hoveredCell.accessRight : 0

        property bool interactiveCells: control.editingEnabled
            && control.mode === PermissionsTab.AdvancedMode

        property color tableHoverColor: interactiveCells
            ? ColorTheme.colors.dark6
            : ColorTheme.colors.dark8

        readonly property real scrollBarWidth: scrollBarVisible ? scrollBar.width : 0

        property int nextBatchCheckState: Qt.Checked

        property var currentSearchRegExp: null

        onFilterTextChanged:
        {
            // Search happens in an escaped text, so also escape filter text here.
            const escaped = tree.filterText
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")

            tree.currentSearchRegExp = escaped
                ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(escaped)})`, 'i')
                : ""
        }

        anchors.fill: parent
        anchors.topMargin: accessRightsHeader.height - 4
        anchors.rightMargin: 8 - scrollBarWidth
        anchors.leftMargin: 16

        expandsOnDoubleClick: false
        selectionEnabled: true
        showResourceStatus: false
        clipDelegates: false
        topMargin: 1
        spacing: 1

        inactiveSelectionHighlightColor: selectionHighlightColor
        hoverHighlightColor: ColorTheme.colors.dark8

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
        webPagesAndIntegrationsCombined: true

        filterText: searchField.text

        function updateNextBatchCheckState()
        {
            if (!accessRightsHeader.hoveredAccessRight || !control.editingContext)
                return

            const currentCheckState = control.editingContext.combinedOwnCheckState(
                tree.selection(), accessRightsHeader.hoveredAccessRight)

            nextBatchCheckState = currentCheckState === Qt.Checked
                ? Qt.Unchecked
                : Qt.Checked;
        }

        onSelectionChanged:
            updateNextBatchCheckState()

        Connections
        {
            target: accessRightsHeader
            function onHoveredAccessRightChanged() { tree.updateNextBatchCheckState() }
        }

        Connections
        {
            target: control.editingContext
            function onResourceAccessChanged() { tree.updateNextBatchCheckState() }
            function onSubjectChanged() { tree.clearSelection(/*clearCurrentIndex*/ true) }
        }

        Binding
        {
            target: tree.model
            property: "externalFilter"
            value: control.editingContext && control.editingContext.accessibleByPermissionsFilter
            when: control.editingContext && filterButton.withPermissionsOnly
        }

        delegate: ResourceAccessDelegate
        {
            id: rowAccess

            editingContext: control.editingContext

            showExtraInfo: tree.model.extraInfoRequired
                || tree.model.isExtraInfoForced(resource)

            selectionMode: tree.selectionMode
            hoverColor: tree.tableHoverColor
            showResourceStatus: tree.showResourceStatus
            columnWidth: accessRightsHeader.columnWidth
            implicitHeight: control.kRowHeight
            editingEnabled: tree.interactiveCells
            automaticDependencies: automaticDependenciesSwitch.checked
            accessRightsList: control.availableAccessRights
            highlightRegExp: tree.currentSearchRegExp

            hoveredColumnAccessRight: accessRightsHeader.hoveredAccessRight
            isRowHovered: tree.hoveredItem === this

            externalNextCheckState: tree.nextBatchCheckState

            externallyHoveredGroups:
            {
                if (tree.hoveredRow)
                {
                    return {
                        "indexes": [tree.hoveredRow.resourceTreeIndex],
                        "accessRight": tree.hoveredCell.accessRight,
                        "toggledOn": tree.hoveredCell.toggledOn,
                        "fromSelection": false}
                }

                if (accessRightsHeader.hoveredAccessRight && control.editingContext)
                {
                    const selection = tree.selection()
                    if (!selection.length)
                        return undefined

                    return {
                        "indexes": selection,
                        "accessRight": accessRightsHeader.hoveredAccessRight,
                        "toggledOn": tree.nextBatchCheckState !== Qt.Checked,
                        "fromSelection": true}
                }

                return undefined
            }

            onHoveredCellChanged:
            {
                if (hoveredCell)
                    tree.hoveredRow = rowAccess
                else if (tree.hoveredRow == rowAccess)
                    tree.hoveredRow = null
            }

            onTriggered:
                tree.clearSelection()
        }
    }

    Item
    {
        anchors.left: parent.left
        anchors.top: accessRightsHeader.bottom
        anchors.bottom: parent.bottom
        anchors.right: permissionsBackground.left

        width: searchField.width

        visible: tree.empty

        Placeholder
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -(searchField.y + searchField.height)

            imageSource: "image://svg/skin/user_settings/no_resources.svg"

            text: qsTr("No resources found")
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

        editingEnabled: control.editingEnabled

        onWithPermissionsOnlyChanged:
        {
            if (editingContext)
                editingContext.resetAccessibleByPermissionsFilter()
        }
    }

    // Lines and shadow.

    Rectangle
    {
        x: permissionsBackground.x
        y: permissionsBackground.y
        width: permissionsBackground.width
        height: 1
        color: ColorTheme.colors.dark5
    }

    Rectangle
    {
        x: permissionsBackground.x
        width: 1
        height: permissionsBackground.y + permissionsBackground.height
        color: ColorTheme.colors.dark5
    }

    Rectangle
    {
        x: permissionsBackground.x - 1
        width: 1
        height: permissionsBackground.y + permissionsBackground.height
        color: ColorTheme.colors.dark8
    }

    Rectangle
    {
        y: permissionsBackground.y
        width: control.width
        height: 16
        opacity: MathUtils.bound(0.0, tree.contentY / height, 1.0)

        gradient: Gradient
        {
            GradientStop { position: 0.0; color: ColorTheme.transparent("black", 0.4) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Row
    {
        // Covers dialog button box.
        parent: control.buttonBox
        visible: control.visible

        anchors.top: parent.top
        height: parent.height
        x: 16
        spacing: 16

        Switch
        {
            id: modeSwitch

            anchors.verticalCenter: parent.verticalCenter
            visible: control.editingEnabled

            text: checked ? qsTr("Advanced Mode On") : qsTr("Advanced Mode Off")
            color: ColorTheme.colors.light10

            onToggled:
                control.mode = (checked ? PermissionsTab.AdvancedMode : PermissionsTab.SimpleMode)

            Binding on checked
            {
                value: control.mode === PermissionsTab.AdvancedMode
            }
        }

        Switch
        {
            id: automaticDependenciesSwitch

            anchors.verticalCenter: parent.verticalCenter

            visible: control.editingEnabled
                && GlobalTemporaries.automaticAccessDependencySwitchVisible

            text: qsTr("Automatically add dependent permissions")

            color: ColorTheme.colors.light10

            readonly property bool windowVisible: Window.visibility !== Window.Hidden

            Component.onCompleted:
                checked = GlobalTemporaries.automaticAccessDependenciesEnabledByDefault

            onWindowVisibleChanged:
                checked = GlobalTemporaries.automaticAccessDependenciesEnabledByDefault

            onToggled:
                GlobalTemporaries.automaticAccessDependenciesEnabledByDefault = checked
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Shift+A"
        enabled: !GlobalTemporaries.automaticAccessDependencySwitchVisible

        onActivated:
            GlobalTemporaries.automaticAccessDependencySwitchVisible = true
    }

    onEditingContextChanged:
        tree.clearSelection(/*clearCurrentIndex*/ true)
}
