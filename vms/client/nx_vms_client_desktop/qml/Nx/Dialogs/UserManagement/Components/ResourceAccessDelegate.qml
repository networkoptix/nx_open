// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Common
import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: root

    required property AccessSubjectEditingContext editingContext
    required property PermissionsOperationContext operationContext
    required property var accessRightDescriptors
    required property bool editingEnabled
    required property bool automaticDependencies
    required property real rehoverDistance
    required property real columnWidth

    property alias showExtraInfo: mainDelegate.showExtraInfo
    property alias selectionMode: mainDelegate.selectionMode
    property alias showResourceStatus: mainDelegate.showResourceStatus

    property alias highlightRegExp: mainDelegate.highlightRegExp

    readonly property alias hoveredCell: cellsRow.hoveredCell
    readonly property var resourceTreeIndex: modelIndex
    readonly property int currentAccessRights: accessRightsModel.grantedAccessRights
    readonly property int relevantAccessRights: accessRightsModel.relevantAccessRights

    readonly property Resource resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    readonly property bool selected: isSelected //< TreeView delegate's context property.

    signal triggered(var effectiveOperationContext)

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

        ignoredProviders: operationContext && operationContext.nextCheckState === Qt.Unchecked
            ? operationContext.selectedLayouts
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

                    property point clickPos

                    readonly property int accessRight: model.accessRight
                    readonly property bool relevant: model.editable

                    GlobalToolTip.enabled: cell.relevant && root.hoveredCell === cell
                        && (operationContext || !root.editingEnabled)

                    Binding on GlobalToolTip.text
                    {
                        value: root.editingEnabled
                            ? cell.normalTooltipText()
                            : cell.readonlyTooltipText()

                        when: cell.GlobalToolTip.enabled
                        restoreMode: Binding.RestoreNone
                    }

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
                        if (!operationContext)
                            return "" //< Should never happen.

                        const name = "<b>" + root.accessRightDescriptors[index].name + "</b>"
                        let operation = ""

                        let inheritance = operationContext.selectedIndexes.length > 1
                            ? ""
                            : model.inheritanceInfoText

                        if (inheritance)
                        {
                            const inheritanceMultiline = !!inheritance.match(/<br>/i)

                            inheritance = inheritanceMultiline
                                ? (":<br>" + inheritance)
                                : (" " + inheritance)
                        }

                        if (operationContext.nextCheckState === Qt.Checked)
                        {
                            if (root.automaticDependencies
                                && (model.requiredAccessRights
                                    & operationContext.relevantAccessRights
                                    & ~operationContext.currentAccessRights))
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
                                && (model.dependentAccessRights
                                    & operationContext.relevantAccessRights
                                    & operationContext.currentAccessRights))
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

                    readonly property int checkState:
                    {
                        switch (accessRightsModel.rowType)
                        {
                            case ResourceAccessTreeItem.Type.resource:
                            {
                                if (model.providedVia === ResourceAccessInfo.ProvidedVia.own
                                    || model.providedVia
                                        === ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                                {
                                    return Qt.Checked
                                }

                                return Qt.Unchecked
                            }

                            case ResourceAccessTreeItem.Type.specialResourceGroup:
                            {
                                if (model.providedVia === ResourceAccessInfo.ProvidedVia.own)
                                    return Qt.Checked

                                return model.checkedChildCount
                                    ? Qt.PartiallyChecked
                                    : Qt.Unchecked
                            }

                            default:
                            {
                                if (model.checkedChildCount === model.totalChildCount)
                                    return Qt.Checked

                                return model.checkedChildCount
                                    ? Qt.PartiallyChecked
                                    : Qt.Unchecked
                            }
                        }
                    }

                    readonly property int displayedProvider:
                    {
                        if (!highlighted || !operationContext)
                            return model.providedVia

                        return operationContext.nextCheckState === Qt.Checked
                            ? ResourceAccessInfo.ProvidedVia.own
                            : model.inheritedFrom
                    }

                    // Highlighted to indicate that a mouse click at current mouse position
                    // shall affect this cell.
                    readonly property bool highlighted:
                    {
                        const context = root.editingContext
                        if (!(context && relevant && operationContext && editingEnabled))
                            return false

                        if (accessRight === operationContext.accessRight)
                            return checkState !== operationContext.nextCheckState

                        if (!root.automaticDependencies)
                            return false

                        if ((accessRightsModel.relevantAccessRights & operationContext.accessRight)
                            && operationContext.nextCheckState !== checkState)
                        {
                            return (operationContext.nextCheckState === Qt.Unchecked)
                                ? context.isDependingOn(accessRight, operationContext.accessRight)
                                : context.isRequiredFor(accessRight, operationContext.accessRight)
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

                        return root.directlySelected
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

                    onClicked: (mouse) =>
                    {
                        if (!cell.relevant
                            || !operationContext
                            || !root.editingEnabled
                            || cell !== root.hoveredCell)
                        {
                            return
                        }

                        // Make one-level-deep copy of the operation context.
                        const effectiveOperationContext = Object.assign({}, operationContext)

                        // Current operation context is cleaned up here, hence the copying above.
                        // Hover must be cleaned before actual toggle operation for proper update
                        // of tooltips: they shouldn't display opposite operation during fade out.
                        cellsRow.hoveredCell = null

                        cell.clickPos = Qt.point(mouse.x, mouse.y)
                        root.triggered(effectiveOperationContext)
                    }

                    onPositionChanged: (mouse) =>
                    {
                        if (cellsRow.hoveredCell || mouse.buttons != 0)
                            return

                        if (Math.abs(mouse.x - clickPos.x) + Math.abs(mouse.y - clickPos.y)
                            >= rehoverDistance)
                        {
                            cellsRow.hoveredCell = cell
                        }
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

                        ColoredImage
                        {
                            id: icon

                            anchors.centerIn: parent

                            sourceSize: Qt.size(20, 20)

                            sourcePath:
                            {
                                switch (cell.displayedProvider)
                                {
                                    case ResourceAccessInfo.ProvidedVia.own:
                                    case ResourceAccessInfo.ProvidedVia.ownResourceGroup:
                                        return "image://skin/20x20/Outline/check.svg"

                                    case ResourceAccessInfo.ProvidedVia.layout:
                                        return "image://skin/20x20/Solid/layout.svg"

                                    case ResourceAccessInfo.ProvidedVia.videowall:
                                        return "image://skin/20x20/Solid/videowall.svg"

                                    case ResourceAccessInfo.ProvidedVia.parentUserGroup:
                                        return "image://skin/20x20/Solid/group.svg"

                                    default:
                                        return ""
                                }
                            }

                            primaryColor: cell.foregroundColor
                        }

                        Text
                        {
                            id: checkedChildResourceCounter

                            height: parent.height
                            width: icon.sourcePath ? icon.x : parent.width

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
