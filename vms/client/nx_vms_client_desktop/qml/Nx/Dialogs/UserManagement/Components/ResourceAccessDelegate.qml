// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls.impl

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

    required property real columnWidth

    required property var editingContext

    required property bool rowHovered
    required property int hoveredAccessRight

    property bool editingEnabled: true

    property bool frameSelectionActive: false
    property rect frameSelectionRect

    property var hoveredCell: null

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    implicitWidth: mainDelegate.implicitWidth
    implicitHeight: 28

    enabled: editingEnabled && !frameSelectionActive

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: delegateRoot.width - cellsRow.width - 1
        height: delegateRoot.height

        highlighted: delegateRoot.enabled
            && !delegateRoot.frameSelectionActive
            && (delegateRoot.rowHovered || delegateRoot.hoveredCell)

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
                    acceptedButtons: cell.effectivelyToggleable ? Qt.LeftButton : Qt.NoButton

                    GlobalToolTip.text:
                    {
                        if (cell.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup
                            && delegateRoot.editingEnabled)
                        {
                            return qsTr("Access granted by parent node. Deselect it to enable editing")
                        }

                        return model.toolTip
                    }

                    readonly property bool toggleable:
                        (accessRightsModel.resource || accessRightsModel.isResourceGroup)
                        && model.editable

                    readonly property bool effectivelyToggleable:
                    {
                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return false

                        return delegateRoot.editingEnabled && cell.toggleable
                    }

                    readonly property int accessRight: model.accessRight

                    readonly property bool frameSelected: delegateRoot.resource
                        && Geometry.intersects(frameSelectionRect, cellsRow.mapToItem(delegateRoot,
                            cell.x, cell.y, cell.width, cell.height))

                    readonly property int providedVia:
                    {
                        return frameSelected && effectivelyToggleable
                            ? ResourceAccessInfo.ProvidedVia.own
                            : model.providedVia
                    }

                    readonly property bool highlighted: frameSelected
                        || (delegateRoot.enabled
                            && (containsMouse
                                || delegateRoot.rowHovered
                                || delegateRoot.hoveredAccessRight == accessRight))

                    readonly property color backgroundColor:
                    {
                        if (!delegateRoot.editingEnabled)
                            return ColorTheme.colors.dark7

                        if (cell.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return ColorTheme.transparent(ColorTheme.colors.brand_core, 0.12)

                        if (cell.providedVia == ResourceAccessInfo.ProvidedVia.own)
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
                            if (cell.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                                return ColorTheme.colors.dark13

                            return cell.providedVia == ResourceAccessInfo.ProvidedVia.own
                                ? ColorTheme.colors.brand_core
                                : ColorTheme.colors.light16
                        }

                        if (cell.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return ColorTheme.transparent(ColorTheme.colors.light13, 0.3)

                        if (cell.providedVia == ResourceAccessInfo.ProvidedVia.own)
                        {
                            return cell.highlighted
                                ? ColorTheme.colors.light4
                                : ColorTheme.colors.light10
                        }

                        return cell.highlighted
                            ? ColorTheme.colors.dark14
                            : ColorTheme.colors.dark13
                    }

                    onContainsMouseChanged:
                    {
                        if (containsMouse)
                            delegateRoot.hoveredCell = cell
                        else if (delegateRoot.hoveredCell == cell)
                            delegateRoot.hoveredCell = null
                    }

                    onClicked:
                    {
                        accessRightsModel.toggle(index)
                    }

                    Item
                    {
                        anchors.fill: cell

                        visible: cell.toggleable

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

                            Text
                            {
                                id: checkedChildResourceCounter

                                anchors.fill: parent
                                horizontalAlignment: Qt.AlignHCenter
                                verticalAlignment: Qt.AlignVCenter

                                visible: accessRightsModel.isResourceGroup
                                    && cell.providedVia != ResourceAccessInfo.ProvidedVia.own
                                    && cell.providedVia !=
                                            ResourceAccessInfo.ProvidedVia.ownResourceGroup
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
}
