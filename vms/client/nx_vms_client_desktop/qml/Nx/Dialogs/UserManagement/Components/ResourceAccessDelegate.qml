// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls
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

    property var accessRightDescriptors: []

    property alias highlightRegExp: mainDelegate.highlightRegExp

    required property real columnWidth

    required property var editingContext

    property bool editingEnabled: true

    property bool automaticDependencies: false

    readonly property alias hoveredCell: cellsRow.hoveredCell

    readonly property var resourceTreeIndex: modelIndex

    readonly property bool selected: isSelected //< TreeView delegate's context property.

    property int hoveredColumnAccessRight: 0
    property int externalNextCheckState: Qt.Checked

    readonly property int externallySelectedAccessRight: (selected || parentNodeSelected)
        ? hoveredColumnAccessRight
        : 0

    property int selectionSize: 0

    property var externalHoverData //< {indexes[], accessRight, toggledOn, fromSelection}
    property int externalRelevantAccessRights: 0

    property var externallySelectedLayouts

    readonly property int relevantAccessRights:
        externalRelevantAccessRights | accessRightsModel.relevantAccessRights

    property bool parentNodeSelected: false

    readonly property bool parentNodeHovered:
    {
        if (!externalHoverData)
            return false

        for (let parent of externalHoverData.indexes)
        {
            if (parent.valid && NxGlobals.isRecursiveChildOf(root.resourceTreeIndex, parent))
                return true
        }

        return false
    }

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    signal triggered(var cell)

    implicitWidth: mainDelegate.implicitWidth + cellsRow.width + 1
    implicitHeight: 28

    enabled: editingEnabled

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: root.width - cellsRow.width
        height: root.height

        highlighted: !!cellsRow.hoveredCell || isHovered //< TreeView delegate's context property.

        customSelectionIndicator: true
        wholeRowToggleable: false
        clip: true
    }

    ResourceAccessRightsModel
    {
        id: accessRightsModel

        context: root.editingContext
        resourceTreeIndex: root.resourceTreeIndex
        accessRightsList: root.accessRightDescriptors.map(item => item.accessRight)

        // If a resource selection contains both layouts and resources that are their items,
        // and a remove operation is highlighted, then the future state inheritance for those
        // resources should not display layouts as they're getting the permission removed as well.

        ignoredProviders:
            root.externallySelectedAccessRight && root.externalNextCheckState == Qt.Unchecked
                ? root.externallySelectedLayouts
                : undefined
    }

    Row
    {
        id: cellsRow

        x: mainDelegate.width
        spacing: 0

        property Item hoveredCell: null

        Repeater
        {
            model: accessRightsModel

            delegate: Component
            {
                MouseArea
                {
                    id: cell

                    width: root.columnWidth
                    height: root.height + 1

                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton

                    GlobalToolTip.enabled: cell.relevant

                    GlobalToolTip.text:
                        root.editingEnabled ? normalTooltipText() : readonlyTooltipText()

                    function readonlyTooltipText()
                    {
                        const name = "<b>" + root.accessRightDescriptors[index].name + "</b>"
                        let inheritance = model.inheritanceInfoText

                        if (inheritance)
                        {
                            const inheritanceMultiline = !!inheritance.match(/<br>/i)

                            inheritance = inheritanceMultiline
                                ? (":<br>" + inheritance)
                                : (" " + inheritance)
                        }

                        if (model.providedVia === ResourceAccessInfo.ProvidedVia.ownResourceGroup
                            || model.providedVia === ResourceAccessInfo.ProvidedVia.own)
                        {
                            const ownPermissionText = qsTr("Has %1 permission",
                                "%1 will be substituted with a permission name").arg(name)

                            if (!inheritance)
                                return ownPermissionText

                            return ownPermissionText + "<br><br>"
                                + qsTr("Also inherits it from", "'it' refers to a permission")
                                + inheritance
                        }

                        if (!inheritance)
                            return ""

                        return qsTr("Inherits %1 permission from",
                            "%1 will be substituted with a permission name").arg(name)
                                + inheritance
                    }

                    function normalTooltipText()
                    {
                        const name = "<b>" + root.accessRightDescriptors[index].name + "</b>"
                        let inheritance = (root.selectionSize > 1) ? "" : model.inheritanceInfoText
                        let operation = ""

                        if (inheritance)
                        {
                            const inheritanceMultiline = !!inheritance.match(/<br>/i)

                            inheritance = inheritanceMultiline
                                ? (":<br>" + inheritance)
                                : (" " + inheritance)
                        }

                        const nextCheckState = root.hoveredColumnAccessRight
                            ? root.externalNextCheckState
                            : (toggledOn ? Qt.Unchecked : Qt.Checked)

                        if (nextCheckState == Qt.Checked)
                        {
                            if (root.automaticDependencies
                                && (model.requiredAccessRights & root.relevantAccessRights))
                            {
                                operation = qsTr("Add %1 and dependent permissions",
                                    "%1 will be substituted with a permission name").arg(name)
                            }
                            else
                            {
                                operation = qsTr("Add %1 permission",
                                    "%1 will be substituted with a permission name").arg(name)
                            }

                            if (inheritance)
                            {
                                inheritance = "<br><br>" + qsTr("Already inherited from")
                                    + inheritance
                            }
                        }
                        else
                        {
                            if (root.automaticDependencies
                                && (model.dependentAccessRights & root.relevantAccessRights))
                            {
                                operation = qsTr("Remove %1 and dependent permissions",
                                    "%1 will be substituted with a permission name").arg(name)
                            }
                            else
                            {
                                operation = qsTr("Remove %1 permission",
                                    "%1 will be substituted with a permission name").arg(name)
                            }

                            if (inheritance)
                            {
                                inheritance = "<br><br>" + qsTr("Will stay inherited from")
                                    + inheritance
                            }
                        }

                        return operation + inheritance
                    }

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

                        const hd = root.externalHoverData

                        if (hd
                            && root.parentNodeHovered
                            && hd.toggledOn === toggledOn
                            && (hd.fromSelection || root.editingEnabled)
                            && (accessRightsModel.relevantAccessRights & hd.accessRight))
                        {
                            // Highlight a child of a hovered resource group cell.
                            if (hd.accessRight === accessRight)
                                return true

                            if (root.automaticDependencies)
                            {
                                if (toggledOn && context.isDependingOn(accessRight, hd.accessRight)
                                    || !toggledOn && context.isRequiredFor(accessRight,
                                        hd.accessRight))
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

                    readonly property color backgroundColor: cell.highlighted
                        ? ColorTheme.colors.brand_d2
                        : "transparent"

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

                        return root.selected
                            ? ColorTheme.colors.light10
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
                        if (!cell.relevant || !root.editingEnabled || cell !== root.hoveredCell)
                            return

                        root.triggered(cell)
                        cellsRow.hoveredCell = null
                    }

                    Rectangle
                    {
                        id: backgroundRect

                        anchors.fill: cell
                        anchors.rightMargin: 1
                        anchors.bottomMargin: 1

                        color: cell.backgroundColor
                        visible: cell.relevant

                        Rectangle
                        {
                            id: frameRect

                            x: -1
                            y: -1
                            width: backgroundRect.width + 2
                            height: backgroundRect.height + 2
                            color: "transparent"
                            border.color: ColorTheme.colors.dark5
                        }

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
                                && cell.displayedProvider === ResourceAccessInfo.ProvidedVia.none
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
