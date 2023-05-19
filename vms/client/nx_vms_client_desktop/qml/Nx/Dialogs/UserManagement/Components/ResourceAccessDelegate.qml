// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

Item
{
    id: delegateRoot

    property alias showExtraInfo: mainDelegate.showExtraInfo
    property alias selectionMode: mainDelegate.selectionMode
    property alias showResourceStatus: mainDelegate.showResourceStatus

    property alias accessRightsList: accessRightsModel.accessRightsList

    required property real columnWidth

    required property var editingContext

    required property var rowHovered
    required property int hoveredAccessRight

    property var hoveredCell: null

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1

    implicitWidth: mainDelegate.implicitWidth
    implicitHeight: 28

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: delegateRoot.width - cellsRow.width - 1
        height: delegateRoot.height

        highlighted: delegateRoot.enabled && (delegateRoot.rowHovered || delegateRoot.hoveredCell)
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

                    acceptedButtons:
                    {
                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup)
                            return Qt.NoButton

                        return delegateRoot.enabled && cell.toggleable
                            ? Qt.LeftButton
                            : Qt.NoButton
                    }

                    GlobalToolTip.text:
                    {
                        if (model.providedVia == ResourceAccessInfo.ProvidedVia.ownResourceGroup
                            && delegateRoot.enabled)
                        {
                            return qsTr("Access granted by parent node. Deselect it to enable editing")
                        }

                        return model.toolTip
                    }

                    readonly property bool toggleable:
                        (accessRightsModel.resource || accessRightsModel.isResourceGroup)
                        && model.editable

                    readonly property int accessRight: model.accessRight

                    readonly property bool highlighted: delegateRoot.enabled && (containsMouse
                        || delegateRoot.rowHovered
                        || delegateRoot.hoveredAccessRight == accessRight)

                    readonly property color backgroundColor:
                    {
                        if (!delegateRoot.enabled)
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
                        if (!delegateRoot.enabled)
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

                            border.color: delegateRoot.enabled
                                ? ColorTheme.colors.dark4
                                : ColorTheme.colors.dark6
                        }

                        IconImage
                        {
                            anchors.centerIn: parent

                            sourceSize: Qt.size(20, 20)

                            source:
                            {
                                switch (model.providedVia)
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
                                    && model.providedVia != ResourceAccessInfo.ProvidedVia.own
                                    && model.providedVia !=
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
