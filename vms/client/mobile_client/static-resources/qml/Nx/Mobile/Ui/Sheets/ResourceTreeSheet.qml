// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Items
import Nx.Core.Controls
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Mobile.Ui
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

Sheet
{
    id: control

    title: qsTr("Resources")

    property bool pendingDefaultExpand: true
    property bool isFiltering: false
    readonly property bool isNothingFound: treeView.rows === 0

    ColumnLayout
    {
        anchors.fill: parent

        SearchEdit
        {
            id: searchEdit

            Layout.fillWidth: true
            implicitHeight: 36

            Layout.leftMargin: 16
            Layout.rightMargin: 16

            readonly property string text: searchEdit.displayText.trim()

            onTextChanged:
            {
                if (text.length !== 0)
                {
                    if (!isFiltering)
                    {
                        treeView.saveExpandedState()
                        isFiltering = true
                    }
                    searchModel.filterRegularExpression
                        = new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(text)})`, 'i')
                    Qt.callLater(treeView.expandAll)
                }
                else
                {
                    searchModel.filterRegularExpression = new RegExp()
                    isFiltering = false
                    Qt.callLater(treeView.restoreExpandedState)
                }
            }

            onAccepted:
            {
                if (isFiltering)
                    treeView.expandAll()
            }
        }

        Item
        {
            id: nothingFoundPlaceholder

            Layout.fillWidth: true
            Layout.fillHeight: true

            visible: isNothingFound

            Placeholder
            {
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                Layout.alignment: Qt.AlignHCenter
                imageSource: "image://skin/64x64/Outline/notfound.svg?primary=light10"
                text: qsTr("Nothing Found")
            }
        }

        ScrollView
        {
            id: scrollView

            Layout.fillWidth: true
            Layout.fillHeight: true

            leftPadding: 16
            rightPadding: 16
            topPadding: 8

            visible: !isNothingFound

            TreeView
            {
                id: treeView
                model: searchModel

                width: parent.width
                clip: true
                rowSpacing: 4

                // Source model indexes to which the view was expanded on the most recent
                // saveExpandedState() call.
                property var storedExpandedState: []

                delegate: ResourceTreeDelegate
                {
                    id: delegateItem

                    highlightRegExp:
                    {
                        if (control.isFiltering)
                            return searchModel.filterRegularExpression
                    }

                    TapHandler
                    {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped:
                        {
                            if (hasChildren)
                            {
                                treeView.toggleExpanded(row)
                            }
                            else if (delegateItem.isCamera)
                            {
                                windowContext.depricatedUiController.layout = null
                                Workflow.openVideoScreen(resource);
                                control.close()
                            }
                            else if (delegateItem.isLayout || delegateItem.isAllDevices)
                            {
                                Workflow.openResourcesScreen()
                                windowContext.depricatedUiController.layout = delegateItem.isLayout ? resource : null
                                control.close()
                            }
                        }
                    }
                }

                function saveExpandedState()
                {
                    storedExpandedState.length = 0
                    for (let row = 0; row < rows; ++row)
                    {
                        storedExpandedState.push(NxGlobals.toPersistent(
                            searchModel.mapToSource(treeView.index(row, 0))))
                    }
                }

                function restoreExpandedState()
                {
                    if (storedExpandedState.length === 0)
                        return
                    collapseRecursively(-1) //< Collapse tree recursively.
                    storedExpandedState.forEach(index =>
                        expandToIndex(searchModel.mapFromSource(NxGlobals.fromPersistent(index))))
                    storedExpandedState.length = 0
                }

                function expandAll()
                {
                    expandRecursively(-1, -1)
                }

                function expandDefault()
                {
                    collapseRecursively(-1) //< Collapse tree recursively.
                    expandRecursively(-1, 1); //< Expand tree to depth 1.
                    if (windowContext.depricatedUiController.layout)
                    {
                        expandToIndex(searchModel.mapFromSource(
                            treeModel.resourceIndex(windowContext.depricatedUiController.layout)))
                    }
                }
            }
        }
    }

    onClosed:
    {
        searchEdit.clear()
        treeView.forceActiveFocus()
    }

    onAboutToShow:
    {
        if (pendingDefaultExpand)
        {
            treeView.expandDefault()
            pendingDefaultExpand = false
        }
    }

    ResourceTreeModel
    {
        id: treeModel
        onModelReset:
        {
            pendingDefaultExpand = true
        }
    }

    ResourceTreeSearchModel
    {
        id: searchModel
        sourceModel: treeModel
    }
}
