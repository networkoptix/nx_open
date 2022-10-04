// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.desktop 1.0

Item
{
    id: delegateRoot

    property alias showExtraInfo: mainDelegate.showExtraInfo
    property alias selectionMode: mainDelegate.selectionMode
    property alias showResourceStatus: mainDelegate.showResourceStatus

    required property real columnWidth

    required property var editingContext

    required property var rowHovered
    required property int hoveredAccessRight

    property var hoveredCell: null

    readonly property var resource: (model && model.resource) || null
    readonly property var nodeType: (model && model.nodeType) || -1
    readonly property var collapsedNodes: tree ? tree.collapsedNodes : []

    readonly property var kOnlyLiveNodes:
    [
        ResourceTree.NodeType.webPages,
        ResourceTree.NodeType.servers
    ]

    readonly property bool onlyLive: isCollapsed()
        ? kOnlyLiveNodes.includes(0 + nodeType)
        : (resource
            && ((resource.flags & NxGlobals.ServerResourceFlag)
                || (resource.flags == NxGlobals.WebPageResourceFlag)))

    function isCollapsed()
    {
        return Array.prototype.includes.call(delegateRoot.collapsedNodes, delegateRoot.nodeType)
    }

    implicitWidth: mainDelegate.implicitWidth
    implicitHeight: 28

    ResourceSelectionDelegate
    {
        id: mainDelegate

        width: delegateRoot.width - cellsRow.width
        height: delegateRoot.height

        highlighted: delegateRoot.rowHovered || delegateRoot.hoveredCell || allCheckBox.hovered
        customSelectionIndicator: true
        wholeRowToggleable: false

        MouseArea
        {
            id: allAccessRightsMouseArea
            anchors.fill: parent

            onClicked:
            {
                infoProvider.toggleAll()
            }
        }

        Row
        {
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.baseline: parent.baseline

            visible: delegateRoot.nodeType != -1

            spacing: 6

            baselineOffset: allCheckBoxText.baselineOffset

            Text
            {
                id: allCheckBoxText

                text: qsTr("All")

                font: Qt.font({pixelSize: 14, weight: Font.Normal})
                color: mainDelegate.highlighted
                    ? ColorTheme.colors.light10
                    : ColorTheme.colors.light16
            }

            CheckBox
            {
                id: allCheckBox

                checked: delegateRoot.isCollapsed()

                nextCheckState: () =>
                {
                    const nodeTypes = tree.collapsedNodes

                    if (checkState === Qt.Unchecked)
                    {
                        nodeTypes.push(delegateRoot.nodeType)
                        tree.collapsedNodes = nodeTypes
                        return Qt.Checked
                    }

                    const index = nodeTypes.indexOf(delegateRoot.nodeType)
                    if (index !== -1)
                        nodeTypes.splice(index, 1)

                    tree.collapsedNodes = nodeTypes

                    return Qt.Unchecked
                }
            }
        }
    }

    ResourceAccessInfoProvider
    {
        id: infoProvider

        context: delegateRoot.editingContext
        resource: delegateRoot.resource
        nodeType: delegateRoot.nodeType
        accessRightsList: Array.prototype.map.call(AccessRightsList.items, item => item.accessRight)
        collapsed: delegateRoot.isCollapsed()
    }

    Row
    {
        id: cellsRow

        x: mainDelegate.width

        Repeater
        {
            model: infoProvider

            delegate: Component
            {
                MouseArea
                {
                    id: cell

                    width: delegateRoot.columnWidth
                    height: delegateRoot.height

                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton

                    readonly property int accessRight: infoProvider.accessRightsList[index]

                    GlobalToolTip.text: model.toolTip

                    readonly property bool highlighted: containsMouse
                        || delegateRoot.rowHovered
                        || delegateRoot.hoveredAccessRight == accessRight

                    onContainsMouseChanged:
                    {
                        if (containsMouse)
                            delegateRoot.hoveredCell = cell
                        else if (delegateRoot.hoveredCell == cell)
                            delegateRoot.hoveredCell = null
                    }

                    onClicked:
                    {
                        model.isOwn = !model.isOwn
                    }

                    Item
                    {
                        anchors.fill: cell

                        visible: (!!delegateRoot.resource || delegateRoot.isCollapsed())
                            && (!delegateRoot.onlyLive || index == 0)

                        Rectangle
                        {
                            x: 1
                            y: 1
                            width: cell.width - 1
                            height: cell.height - 1

                            color: cell.highlighted
                                ? ColorTheme.colors.dark8
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
                                        return "image://svg/skin/user_settings/sharing/own.svg"

                                    case ResourceAccessInfo.ProvidedVia.layout:
                                        return "image://svg/skin/user_settings/sharing/layout.svg"

                                    case ResourceAccessInfo.ProvidedVia.videowall:
                                        return "image://svg/skin/user_settings/sharing/videowall.svg"

                                    case ResourceAccessInfo.ProvidedVia.parent:
                                        return "image://svg/skin/user_settings/sharing/group.svg"

                                    default:
                                        return ""
                                }
                            }

                            color: model.providedVia == ResourceAccessInfo.ProvidedVia.own
                                ? ColorTheme.colors.brand_core
                                : ColorTheme.colors.light10
                        }
                    }
                }
            }
        }
    }
}
