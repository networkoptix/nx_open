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
    id: delegateRoot

    property alias showExtraInfo: mainDelegate.showExtraInfo
    property alias selectionMode: mainDelegate.selectionMode
    property alias showResourceStatus: mainDelegate.showResourceStatus

    property alias accessRightsList: accessRightsModel.accessRightsList

    property alias highlightRegExp: mainDelegate.highlightRegExp

    required property real columnWidth

    required property var editingContext

    property bool editingEnabled: true

    property bool automaticDependencies: false

    enum FrameSelectionMode
    {
        NoFrameOperation,
        FrameSelection,
        FrameUnselection
    }

    property int frameSelectionMode: ResourceAccessDelegate.NoFrameOperation
    property rect frameSelectionRect

    property var hoveredCell: null

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    implicitWidth: mainDelegate.implicitWidth
    implicitHeight: 28

    enabled: editingEnabled && frameSelectionMode == ResourceAccessDelegate.NoFrameOperation

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: delegateRoot.width - cellsRow.width - 1
        height: delegateRoot.height

        highlighted: delegateRoot.enabled
            && frameSelectionMode == ResourceAccessDelegate.NoFrameOperation
            && delegateRoot.hoveredCell

        customSelectionIndicator: true
        wholeRowToggleable: false
        clip: true
    }

    ResourceAccessRightsModel
    {
        id: accessRightsModel

        context: delegateRoot.editingContext
        resource: delegateRoot.resource
        nodeType: delegateRoot.nodeType
    }

    Row
    {
        id: cellsRow

        x: mainDelegate.width

        readonly property int frameAccessRights: Array.prototype.reduce.call(
            children,
            (total, child) => child.frameSelected ? (total | child.accessRight) : total,
            0)

        Repeater
        {
            model: accessRightsModel

            delegate: Component
            {
                MouseArea
                {
                    id: cell

                    width: delegateRoot.columnWidth
                    height: delegateRoot.height

                    hoverEnabled: true
                    acceptedButtons: cell.toggleable ? Qt.LeftButton : Qt.NoButton

                    GlobalToolTip.enabled:
                        frameSelectionMode == ResourceAccessDelegate.NoFrameOperation

                    GlobalToolTip.text: model.toolTip

                    readonly property int accessRight: model.accessRight

                    readonly property bool relevant:
                        (accessRightsModel.resource || accessRightsModel.isResourceGroup)
                        && model.editable

                    readonly property bool toggleable:
                    {
                        return delegateRoot.editingEnabled && cell.relevant
                            && model.providedVia != ResourceAccessInfo.ProvidedVia.ownResourceGroup
                    }

                    readonly property bool frameSelected: toggleable && delegateRoot.resource
                        && Geometry.intersects(frameSelectionRect, cellsRow.mapToItem(delegateRoot,
                            cell.x, cell.y, cell.width, cell.height))

                    // Highlighted to indicate that a mouse click at current mouse position
                    // or applying current frame selection shall affect this cell.
                    readonly property bool highlighted:
                    {
                        const context = delegateRoot.editingContext
                        if (!context || !toggleable)
                            return false

                        // Highlight directly hovered cell.
                        if (delegateRoot.enabled && delegateRoot.hoveredCell === cell)
                            return true

                        const toggledOn = isToggledOn()

                        const nextCheckValue =
                            frameSelectionMode != ResourceAccessDelegate.FrameUnselection

                        // Highlight frame-selected cell.
                        if (frameSelected && nextCheckValue != toggledOn)
                            return true

                        if (!delegateRoot.automaticDependencies)
                            return false

                        // Highlight access rights dependency for current frame selection.
                        if (frameSelectionMode != ResourceAccessDelegate.NoFrameOperation
                            && nextCheckValue != toggledOn)
                        {
                            return nextCheckValue
                                ? context.isRequiredFor(accessRight, cellsRow.frameAccessRights)
                                : context.isDependingOn(accessRight, cellsRow.frameAccessRights)
                        }

                        // Highlight access rights dependency from hovered other cell.
                        const base = delegateRoot.hoveredCell

                        if (base && base.toggleable
                            && frameSelectionMode == ResourceAccessDelegate.NoFrameOperation
                            && toggledOn == base.isToggledOn())
                        {
                            return toggledOn
                                ? context.isDependingOn(accessRight, base.accessRight)
                                : context.isRequiredFor(accessRight, base.accessRight)
                        }

                        return false
                    }

                    readonly property color backgroundColor:
                    {
                        if (!delegateRoot.editingEnabled)
                            return ColorTheme.colors.dark7

                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return ColorTheme.transparent(ColorTheme.colors.brand_core, 0.12)

                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.own)
                        {
                            return cell.highlighted
                                ? ColorTheme.transparent(ColorTheme.colors.brand_core, 0.6)
                                : ColorTheme.transparent(ColorTheme.colors.brand_core, 0.4)
                        }

                        return cell.highlighted
                            ? ColorTheme.colors.dark6
                            : ColorTheme.colors.dark5
                    }

                    readonly property color foregroundColor:
                    {
                        if (!delegateRoot.editingEnabled)
                        {
                            if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                                return ColorTheme.colors.dark13

                            return model.providedVia == ResourceAccessInfo.ProvidedVia.own
                                ? ColorTheme.colors.brand_core
                                : ColorTheme.colors.light16
                        }

                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return ColorTheme.transparent(ColorTheme.colors.light13, 0.3)

                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.own)
                        {
                            return cell.highlighted
                                ? ColorTheme.colors.light4
                                : ColorTheme.colors.light10
                        }

                        return cell.highlighted
                            ? ColorTheme.colors.dark14
                            : ColorTheme.colors.dark13
                    }

                    function isToggledOn()
                    {
                        // A group not explicitly toggled on but having all children toggled on
                        // is still considered toggled on, for convenience of access right
                        // dependency tracking.

                        return providedVia == ResourceAccessInfo.ProvidedVia.own
                            || (model.checkedChildCount
                                && model.checkedChildCount == model.totalChildCount)
                    }

                    onContainsMouseChanged:
                    {
                        if (containsMouse)
                            delegateRoot.hoveredCell = cell
                        else if (delegateRoot.hoveredCell == cell)
                            delegateRoot.hoveredCell = null
                    }

                    onClicked:
                        accessRightsModel.toggle(index, delegateRoot.automaticDependencies)

                    Item
                    {
                        anchors.fill: cell

                        visible: cell.relevant

                        Rectangle
                        {
                            width: cell.width + 1
                            height: cell.height + 1
                            color: cell.backgroundColor

                            border.color: delegateRoot.editingEnabled
                                ? ColorTheme.colors.dark4
                                : ColorTheme.colors.dark6
                        }

                        IconImage
                        {
                            id: icon

                            anchors.centerIn: parent

                            sourceSize: Qt.size(20, 20)

                            source:
                            {
                                switch (providedVia)
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

                            visible: accessRightsModel.isResourceGroup
                                && model.providedVia == ResourceAccessInfo.ProvidedVia.none
                                && model.checkedChildCount

                            color: cell.foregroundColor
                            text: model.checkedChildCount
                        }
                    }
                }
            }
        }
    }
}
