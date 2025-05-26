// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Dialogs
import Nx.Models
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

Page
{
    id: organizationScreen
    objectName: "organizationScreen"

    title: subtreeModel.sourceRoot && !searchField.visible
        ? accessor.getData(subtreeModel.sourceRoot, "display")
        : ""
    titleUnderlineVisible: true

    onLeftButtonClicked: goBack()

    customBackHandler: () => goBack()

    property var model
    property var rootIndex

    readonly property bool searching: !!siteList.searchText

    LinearizationListModel
    {
        id: subtreeModel

        sourceModel: model
        sourceRoot: organizationScreen.rootIndex
        autoExpandAll: searching
        onAutoExpandAllChanged:
        {
            if (!autoExpandAll)
                collapseAll()
        }
    }

    ModelDataAccessor
    {
        id: accessor
        model: organizationScreen.model
    }

    rightControl: IconButton
    {
        id: searchButton

        anchors.centerIn: parent

        icon.source: searchField.visible
            ? "image://skin/24x24/Outline/close.svg?primary=light10"
            : "image://skin/24x24/Outline/search.svg?primary=light10"
        icon.width: 24
        icon.height: 24

        onClicked: searchField.visible ? goBack() : startSearch()
        alwaysCompleteHighlightAnimation: false
    }

    centerControl:
    [
        MouseArea
        {
            anchors.fill: parent
            visible: !searchField.visible && !breadcrumb.visible
            onClicked: breadcrumb.openWith(subtreeModel.sourceRoot)
        },

        SearchEdit
        {
            id: searchField

            visible: false
            width: parent.width
            height: 36
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: qsTr("Search")
        }
    ]

    function startSearch()
    {
        if (searchField.visible)
            return

        siteList.currentRoot = subtreeModel.sourceRoot
        subtreeModel.sourceRoot = Qt.binding(
            () => searching ? NxGlobals.invalidModelIndex() : siteList.currentRoot)
        searchField.visible = true
        searchField.forceActiveFocus()
    }

    function endSearch()
    {
        if (!searchField.visible)
            return

        subtreeModel.sourceRoot = siteList.currentRoot
        searchField.visible = false
        searchField.clear()
        searchField.resetFocus()
        siteList.positionViewAtBeginning()
    }

    function currentTopLevelIndex()
    {
        let orgOrPartner = null

        for (let node = rootIndex; node && node.row != -1; node = node.parent)
            orgOrPartner = node

        return orgOrPartner
    }

    function goBack()
    {
        if (searchField.visible)
        {
            endSearch()
            return
        }

        const newIndex = subtreeModel.sourceRoot && subtreeModel.sourceRoot.parent

        if (newIndex.row == -1)
        {
            Workflow.popCurrentScreen()
        }
        else
        {
            subtreeModel.sourceRoot = newIndex
            siteList.currentRoot = newIndex
        }
    }

    function goInto(current)
    {
        endSearch()

        if (accessor.getData(current, "type") != OrganizationsModel.System)
        {
            subtreeModel.sourceRoot = current
            return
        }

        organizationScreen.model.systemOpened(current)
    }

    MouseArea
    {
        id: searchCanceller

        z: 1
        anchors.fill: parent

        enabled: searchField.visible
        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchField.resetFocus()
        }
    }

    Breadcrumb
    {
        id: breadcrumb

        x: siteList.x
        y: siteList.y + 8
        width: siteList.width

        onItemClicked: (nodeId) =>
        {
            goInto(organizationScreen.model.indexFromNodeId(nodeId))
        }

        function openWith(root)
        {
            let data = []

            for (let node = root; node && node.row != -1; node = node.parent)
            {
                data.push({
                    name: accessor.getData(node, "display"),
                    nodeId: accessor.getData(node, "nodeId")
                })
            }

            model = data.reverse()
            open()
        }
    }

    Placeholder
    {
        id: noSitesInOrgPlaceholder

        readonly property bool inOrg: rootIndex && rootIndex.parent.row == -1

        visible: !searching && siteList.count == 0 && !loadingIndicator.visible

        z: 1

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2

        imageSource: "image://skin/64x64/Outline/nosite.svg?primary=light10"
        text: qsTr("No Sites")
        description: inOrg
            ? qsTr("We did not find any sites in this organization")
            : qsTr("We did not find any sites in this folder")
        buttonText: inOrg ? qsTr("How to connect sites?") : ""
        buttonIcon.source: "image://skin/24x24/Outline/cloud.svg?primary=dark1"
        onButtonClicked: { console.log("how to connect sites?"); }
    }

    Placeholder
    {
        id: searchNotFoundPlaceholder

        visible: searching && siteList.count == 0

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -header.height / 2
        imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10"
        text: qsTr("Nothing found")
        description: qsTr("Try changing the search parameters")
    }

    Skeleton
    {
        id: loadingIndicator

        anchors.fill: parent

        visible: accessor.getData(rootIndex, "isLoading") || false
        Connections
        {
            target: accessor
            function updateVisibility()
            {
                loadingIndicator.visible = accessor.getData(rootIndex, "isLoading") || false
            }
            function onDataChanged() { updateVisibility() }
            function onRowsInserted() { updateVisibility() }
            function onRowsRemoved() { updateVisibility() }
        }

        Flow
        {
            anchors.fill: parent
            topPadding: 16
            leftPadding: 20
            spacing: siteList.spacing

            Repeater
            {
                model: 4

                delegate: Rectangle
                {
                    radius: 8
                    color: "white"
                    antialiasing: false
                    width: siteList.cellWidth
                    height: 116
                }
            }
        }
    }

    SiteList
    {
        id: siteList

        anchors.fill: parent

        siteModel: subtreeModel

        PropertyUpdateFilter on searchText
        {
            source: searchField.displayText
            minimumIntervalMs: 250
        }

        onItemClicked: (nodeId, systemId) =>
        {
            goInto(organizationScreen.model.indexFromNodeId(nodeId))
        }
    }
}
