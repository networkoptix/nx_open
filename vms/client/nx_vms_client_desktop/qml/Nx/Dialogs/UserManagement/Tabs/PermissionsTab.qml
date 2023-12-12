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

    readonly property int kRowHeight: 27
    readonly property int kMinColumnWidth: 64

    property var buttonBox

    property bool editingEnabled: true

    property bool advancedMode: true

    readonly property var availableAccessRightDescriptors:
        Array.prototype.slice.call(AccessRightsList.items) //< Deep copy to JS for optimization.

    // Acts as a minimum width for the dialog.
    implicitWidth: Math.max(footerRow.width + control.buttonBox.implicitWidth,
        (kMinColumnWidth * columnCount) + 320 /*some sensible minimum for the tree*/)

    Item
    {
        id: accessRightsHeader

        property int columnWidth: Math.max(control.kMinColumnWidth,
            Math.round((control.width * 0.5) / (control.columnCount * 16)) * 16)

        anchors.right: control.right
        anchors.rightMargin: 9

        height: 66
        width: control.columnCount * columnWidth

        property int hoveredAccessRight: 0

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

                    color:
                    {
                        const highlighted = hovered
                            || (tree.hoveredCell && tree.hoveredCell.accessRight == accessRight)

                        return highlighted ? ColorTheme.colors.light4 : ColorTheme.colors.light10
                    }

                    text: modelData.name
                    interactive: control.editingEnabled && tree.hasSelection

                    GlobalToolTip.text: modelData.description

                    onInteractiveHoveredChanged:
                    {
                        if (interactiveHovered)
                            accessRightsHeader.hoveredAccessRight = accessRight
                        else if (accessRightsHeader.hoveredAccessRight == accessRight)
                            accessRightsHeader.hoveredAccessRight = 0
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

        color: ColorTheme.colors.dark7

        Rectangle
        {
            id: hoveredColumnBackground

            readonly property int index: availableAccessRightDescriptors.findIndex(
                item => item.accessRight === tree.hoveredColumnAccessRight)

            x: index * accessRightsHeader.columnWidth

            width: accessRightsHeader.columnWidth - 1
            height: permissionsBackground.height

            visible: index >= 0
            color: ColorTheme.colors.dark8
        }
    }

    ResourceSelectionTree
    {
        id: tree

        property var hoveredRow: null
        readonly property var hoveredCell: hoveredRow ? hoveredRow.hoveredCell : null

        readonly property int hoveredColumnAccessRight:
        {
            if (accessRightsHeader.hoveredAccessRight)
                return accessRightsHeader.hoveredAccessRight

            if (hoveredRow && (hoveredRow.selected || hoveredRow.parentNodeSelected))
                return hoveredCell.accessRight

            return 0
        }

        readonly property var hoverData: // {indexes[], accessRight, toggledOn, fromSelection}
        {
            if (tree.hoveredColumnAccessRight && control.editingContext)
            {
                const selection = tree.selection()
                if (!selection.length)
                    return undefined

                return {
                    "indexes": selection,
                    "accessRight": tree.hoveredColumnAccessRight,
                    "toggledOn": tree.nextBatchCheckState !== Qt.Checked,
                    "fromSelection": true}
            }

            if (tree.hoveredRow)
            {
                return {
                    "indexes": [tree.hoveredRow.resourceTreeIndex],
                    "accessRight": tree.hoveredCell.accessRight,
                    "toggledOn": tree.hoveredCell.toggledOn,
                    "fromSelection": false}
            }

            return undefined
        }

        property int selectionRelevantAccessRights: 0

        readonly property real scrollBarWidth: scrollBarVisible ? scrollBar.width : 0

        property int nextBatchCheckState: Qt.Checked
        property int selectionSize: 0

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
        anchors.topMargin: accessRightsHeader.height
        anchors.rightMargin: 8 - scrollBarWidth
        anchors.leftMargin: 16

        expandsOnDoubleClick: false
        selectionEnabled: control.editingEnabled
        clickUnselectsSingleSelectedItem: true
        showResourceStatus: false
        clipDelegates: false
        topMargin: 1
        spacing: 1

        selectionHighlightColor: ColorTheme.blend(
            ColorTheme.colors.dark6, ColorTheme.colors.brand_core, 0.4)

        inactiveSelectionHighlightColor: selectionHighlightColor

        hoverHighlightColor:
        {
            if (!hoveredItem)
                return ColorTheme.colors.dark8

            if (hoveredItem.selected)
                return ColorTheme.blend(ColorTheme.colors.dark8, ColorTheme.colors.brand_core, 0.4)

            return hoveredItem.parentNodeSelected
                ? ColorTheme.colors.dark9
                : ColorTheme.colors.dark8
        }

        selectionOverlapsSpacing: true

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

        selectAllSiblingsRoleName: "nodeType"

        function updateSelectionInfo()
        {
            if (!hoveredColumnAccessRight || !control.editingContext)
            {
                selectionRelevantAccessRights = 0
                return
            }

            const selection = tree.selection()

            const currentCheckState = control.editingContext.combinedOwnCheckState(
                selection, hoveredColumnAccessRight)

            selectionSize = selection.length

            nextBatchCheckState = currentCheckState === Qt.Checked
                ? Qt.Unchecked
                : Qt.Checked

            selectionRelevantAccessRights =
                control.editingContext.combinedRelevantAccessRights(selection)
        }

        onSelectionChanged:
            updateSelectionInfo()

        onHoveredColumnAccessRightChanged:
            updateSelectionInfo()

        Connections
        {
            target: control.editingContext
            function onResourceAccessChanged() { tree.updateSelectionInfo() }
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
            showResourceStatus: tree.showResourceStatus
            columnWidth: accessRightsHeader.columnWidth
            implicitHeight: control.kRowHeight
            editingEnabled: control.editingEnabled
            automaticDependencies: automaticDependenciesSwitch.checked
            accessRightDescriptors: control.availableAccessRightDescriptors
            highlightRegExp: tree.currentSearchRegExp
            hoveredColumnAccessRight: tree.hoveredColumnAccessRight
            selectionSize: tree.selectionSize

            externalNextCheckState: tree.nextBatchCheckState
            externalHoverData: tree.hoverData
            externalRelevantAccessRights: tree.selectionRelevantAccessRights

            onHoveredCellChanged:
            {
                if (hoveredCell)
                    tree.hoveredRow = rowAccess
                else if (tree.hoveredRow == rowAccess)
                    tree.hoveredRow = null
            }

            onTriggered: (cell) =>
            {
                if (!control.editingContext)
                    return

                if (rowAccess.selected || rowAccess.parentNodeSelected)
                {
                    control.editingContext.modifyAccessRights(tree.selection(),
                        cell.accessRight,
                        tree.nextBatchCheckState === Qt.Checked,
                        automaticDependenciesSwitch.checked)
                }
                else
                {
                    control.editingContext.modifyAccessRight(modelIndex,
                        cell.accessRight,
                        !cell.toggledOn,
                        automaticDependenciesSwitch.checked)

                    tree.clearSelection(/*clearCurrentIndex*/ true)
                }
            }

            function isParentNodeSelected()
            {
                for (let index = modelIndex.parent; index.valid; index = index.parent)
                {
                    if (tree.isSelected(index))
                        return true
                }

                return false
            }

            onResourceTreeIndexChanged:
                rowAccess.parentNodeSelected = rowAccess.isParentNodeSelected()

            Connections
            {
                target: tree

                function onSelectionChanged()
                {
                    rowAccess.parentNodeSelected = rowAccess.isParentNodeSelected()
                }
            }

            Rectangle
            {
                id: childSelectionHighlight

                parent: rowItem.parent //< rowItem is TreeView delegate's context property.
                anchors.fill: (rowItem.parent === parent) ? rowItem : undefined
                anchors.topMargin: -tree.spacing
                anchors.bottomMargin: -tree.spacing

                z: selectionItem.z - 1 //< selectionItem is TreeView delegate's context property.
                color: ColorTheme.colors.dark8
                visible: rowAccess.parentNodeSelected && !isSelected
            }

            Rectangle
            {
                id: rowHoverMarker

                anchors.fill: rowAccess
                visible: tree.hoveredItem === this && !rawAccess.selected
                color: tree.hoverHighlightColor
                z: -1
            }
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
        color: ColorTheme.colors.dark6
    }

    Rectangle
    {
        x: permissionsBackground.x
        width: 1
        height: permissionsBackground.y + permissionsBackground.height
        color: ColorTheme.colors.dark6
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
            GradientStop
            {
                position: 0.0
                color: ColorTheme.transparent(ColorTheme.colors.dark2, 0.3)
            }

            GradientStop
            {
                position: 1.0
                color: "transparent"
            }
        }
    }

    // A footer over the dialog button box.
    Row
    {
        id: footerRow

        parent: control.buttonBox
        height: parent.height

        visible: control.visible

        leftPadding: 16
        rightPadding: 16
        spacing: 8

        ContextHintButton
        {
            anchors.verticalCenter: footerRow.verticalCenter
            toolTipText: qsTr("Resources table gives you an overview of user or group permissions"
                + " and allows you to assign permissions for specific resources.") + "<br>"
                + qsTr("If you select a permission that depends on another permission both "
                + "permissions will be granted automatically.")
            helpTopic: HelpTopic.UserSettings
        }

        Text
        {
            font.pixelSize: 14
            color: ColorTheme.colors.light16
            height: footerRow.height
            verticalAlignment: Text.AlignVCenter
            visible: control.editingEnabled
                && !GlobalTemporaries.automaticAccessDependencySwitchVisible

            text: qsTr("Use %1 or %2 to select multiple resources, or %3 to clear the selection",
                "%1, %2 and %3 will be replaced with keyboard key names")
                    .arg("<b>" + NxGlobals.modifierName(Qt.ControlModifier) + "</b>")
                    .arg("<b>" + NxGlobals.modifierName(Qt.ShiftModifier) + "</b>")
                    .arg("<b>Esc</b>")
        }

        Switch
        {
            id: automaticDependenciesSwitch

            anchors.verticalCenter: footerRow.verticalCenter

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

    Keys.onShortcutOverride: (event) =>
    {
        if (event.key !== Qt.Key_Escape)
            return

        event.accepted = true
        tree.clearSelection(/*clearCurrentIndex*/ true)
    }

    onEditingContextChanged:
        tree.clearSelection(/*clearCurrentIndex*/ true)

    onVisibleChanged:
    {
        if (visible)
            tree.forceActiveFocus()
    }
}
