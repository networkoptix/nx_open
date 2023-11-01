// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls
import Nx.Items

import nx.client.core
import nx.vms.client.desktop

Item
{
    id: root

    property alias showExtraInfo: mainDelegate.showExtraInfo
    property alias selectionMode: mainDelegate.selectionMode
    property alias showResourceStatus: mainDelegate.showResourceStatus

    property alias accessRightsList: accessRightsModel.accessRightsList

    property alias highlightRegExp: mainDelegate.highlightRegExp

    required property real columnWidth

    required property var editingContext

    property bool editingEnabled: true

    property color hoverColor

    property bool automaticDependencies: false

    readonly property alias hoveredCell: cellsRow.hoveredCell

    readonly property var resourceTreeIndex: modelIndex

    property bool isRowHovered: false
    property int hoveredColumnAccessRight: 0
    property int externalNextCheckState: Qt.Checked

    readonly property int externallySelectedAccessRight: isSelected
        ? hoveredColumnAccessRight
        : 0

    property var externallyHoveredGroups // {indexes[], accessRight, toggledOn, fromSelection}

    readonly property bool isParentHovered:
    {
        if (!externallyHoveredGroups)
            return false

        for (let parent of externallyHoveredGroups.indexes)
        {
            if (NxGlobals.isRecursiveChildOf(root.resourceTreeIndex, parent))
                return true
        }

        return false
    }

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    signal triggered(int index)

    implicitWidth: mainDelegate.implicitWidth + cellsRow.width + 1
    implicitHeight: 28

    enabled: editingEnabled

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: root.width - cellsRow.width - 1
        height: root.height

        highlighted: !!cellsRow.hoveredCell

        customSelectionIndicator: true
        wholeRowToggleable: false
        clip: true
    }

    ResourceAccessRightsModel
    {
        id: accessRightsModel

        context: root.editingContext
        resourceTreeIndex: root.resourceTreeIndex
    }

    Rectangle
    {
        id: rowHoverMarker

        anchors.fill: cellsRow
        visible: root.isRowHovered && !isSelected
        color: root.hoverColor
    }

    Row
    {
        id: cellsRow

        x: mainDelegate.width
        spacing: 1

        property Item hoveredCell: null

        Repeater
        {
            model: accessRightsModel

            delegate: Component
            {
                MouseArea
                {
                    id: cell

                    width: root.columnWidth - 1
                    height: root.height

                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton

                    GlobalToolTip.text: model.toolTip

                    readonly property int accessRight: model.accessRight
                    readonly property bool relevant: model.editable

                    readonly property bool toggledOn:
                    {
                        switch (accessRightsModel.rowType)
                        {
                            case ResourceAccessTreeItem.Type.resource:
                                return model.providedVia === ResourceAccessInfo.ProvidedVia.own
                                    || model.providedVia
                                            === ResourceAccessInfo.ProvidedVia.ownResourceGroup

                            case ResourceAccessTreeItem.Type.specialResourceGroup:
                                return model.providedVia === ResourceAccessInfo.ProvidedVia.own

                            default:
                                return model.checkedChildCount === model.totalChildCount
                        }
                    }

                    readonly property int displayedProvider:
                    {
                        if (!highlighted)
                            return model.providedVia

                        return toggledOn
                            ? model.inheritedFrom
                            : ResourceAccessInfo.ProvidedVia.own
                    }

                    // Highlighted to indicate that a mouse click at current mouse position
                    // shall affect this cell.
                    readonly property bool highlighted:
                    {
                        const context = root.editingContext
                        if (!context || !relevant)
                            return false

                        // Highlight external selection (selected rows & hovered column crossings).
                        if (accessRight === root.externallySelectedAccessRight)
                        {
                            const willBeToggledOn =
                                root.externalNextCheckState === Qt.Checked

                            return toggledOn !== willBeToggledOn
                        }

                        // Highlight directly hovered cell.
                        if (root.editingEnabled && cellsRow.hoveredCell === cell)
                            return true

                        const hg = root.externallyHoveredGroups

                        if (hg
                            && root.isParentHovered
                            && hg.toggledOn === toggledOn
                            && (hg.fromSelection || root.editingEnabled)
                            && (accessRightsModel.relevantAccessRights & hg.accessRight))
                        {
                            // Highlight a child of a hovered resource group cell.
                            if (hg.accessRight === accessRight)
                                return true

                            if (root.automaticDependencies)
                            {
                                if (toggledOn && context.isDependingOn(accessRight, hg.accessRight)
                                    || !toggledOn && context.isRequiredFor(accessRight,
                                        hg.accessRight))
                                {
                                    return true
                                }
                            }
                        }

                        if (!root.automaticDependencies)
                            return false

                        // Highlight dependencies from the external selection.
                        if (root.externallySelectedAccessRight
                            && (accessRightsModel.relevantAccessRights
                                & root.externallySelectedAccessRight))
                        {
                            if (root.externalNextCheckState === Qt.Unchecked)
                            {
                                return toggledOn && context.isDependingOn(
                                    accessRight, root.externallySelectedAccessRight)
                            }
                            else
                            {
                                return !toggledOn && context.isRequiredFor(
                                    accessRight, root.externallySelectedAccessRight)
                            }
                        }

                        // Highlight access rights dependency from hovered other cell.
                        const base = root.editingEnabled ? cellsRow.hoveredCell : null
                        if (base && base.relevant && toggledOn == base.toggledOn)
                        {
                            return toggledOn
                                ? context.isDependingOn(accessRight, base.accessRight)
                                : context.isRequiredFor(accessRight, base.accessRight)
                        }

                        return false
                    }

                    readonly property color backgroundColor:
                    {
                        if (cell.highlighted)
                            return ColorTheme.colors.brand_core

                        if (isSelected || root.isRowHovered)
                            return "transparent"

                        if (cell.relevant && (root.hoveredColumnAccessRight & cell.accessRight))
                            return root.hoverColor

                        return root.editingEnabled
                            ? ColorTheme.colors.dark5
                            : ColorTheme.colors.dark7
                    }

                    readonly property color foregroundColor:
                    {
                        if (cell.highlighted)
                            return ColorTheme.colors.brand_contrast

                        if (cell.displayedProvider
                                === ResourceAccessInfo.ProvidedVia.ownResourceGroup
                            || cell.displayedProvider === ResourceAccessInfo.ProvidedVia.own)
                        {
                            return ColorTheme.colors.brand_core
                        }

                        return isSelected //< TreeView delegate context property.
                            ? ColorTheme.colors.light16
                            : ColorTheme.colors.dark13
                    }

                    onContainsMouseChanged:
                    {
                        if (relevant && containsMouse)
                            cellsRow.hoveredCell = cell
                        else if (cellsRow.hoveredCell == cell)
                            cellsRow.hoveredCell = null
                    }

                    onClicked:
                    {
                        if (!cell.relevant || !root.editingEnabled)
                            return

                        accessRightsModel.toggle(
                            modelIndex, index, root.automaticDependencies)

                        cellsRow.hoveredCell = null
                        root.triggered(index)
                    }

                    Rectangle
                    {
                        id: backgroundRect

                        anchors.fill: cell

                        color: cell.backgroundColor
                        visible: cell.relevant

                        Rectangle
                        {
                            id: frameRect

                            x: -1
                            y: -1
                            width: cell.width + 2
                            height: cell.height + 2
                            color: "transparent"

                            border.color: root.editingEnabled
                                ? ColorTheme.colors.dark4
                                : ColorTheme.colors.dark6
                        }
                    }

                    Item
                    {
                        anchors.fill: cell

                        visible: cell.relevant

                        IconImage
                        {
                            id: icon

                            anchors.centerIn: parent

                            sourceSize: Qt.size(20, 20)

                            source:
                            {
                                switch (cell.displayedProvider)
                                {
                                    case ResourceAccessInfo.ProvidedVia.own:
                                    case ResourceAccessInfo.ProvidedVia.ownResourceGroup:
                                        return "image://svg/skin/user_settings/sharing/own.svg"

                                    case ResourceAccessInfo.ProvidedVia.layout:
                                        return "image://svg/skin/user_settings/sharing/layout.svg"

                                    case ResourceAccessInfo.ProvidedVia.videowall:
                                        return "image://svg/skin/user_settings/sharing/videowall.svg"

                                    case ResourceAccessInfo.ProvidedVia.parentUserGroup:
                                        return "image://svg/skin/user_settings/sharing/group.svg"

                                    default:
                                        return ""
                                }
                            }

                            color: cell.foregroundColor
                        }

                        Text
                        {
                            id: checkedChildResourceCounter

                            height: parent.height
                            width: (icon.source != "") ? icon.x : parent.width

                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter

                            visible: model.checkedAndInheritedChildCount > 0
                                && cell.displayedProvider !== ResourceAccessInfo.ProvidedVia.own
                                && !cell.highlighted

                            color: cell.foregroundColor
                            text: model.checkedAndInheritedChildCount
                        }
                    }
                }
            }
        }
    }
}
