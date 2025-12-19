// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
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

    Column
    {
        width: parent.width
        spacing: 4

        SearchEdit
        {
            id: searchEdit
            height: 36
            width: parent.width

            readonly property string text: searchEdit.displayText
            readonly property var regExp: text.length !== 0
                ? new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(text)})`, 'i')
                : new RegExp()

            onTextChanged:
            {
                Qt.callLater(text.length !== 0
                    ? treeView.expandAll
                    : treeView.expandDefault)
            }
        }

        TreeView
        {
            id: treeView
            model: searchModel

            width: parent.width
            height: control.contentItem.height - control.headerHeight
            rowSpacing: 4
            clip: true

            delegate: ResourceTreeDelegate
            {
                id: delegateItem

                highlightRegExp:
                {
                    if (searchEdit.text.length !== 0)
                        return searchEdit.regExp
                }

                onClicked:
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
                    else if (delegateItem.isLayout)
                    {
                        Workflow.openResourcesScreen()
                        windowContext.depricatedUiController.layout = resource
                        control.close()
                    }
                    else if (delegateItem.isAllDevices)
                    {
                        Workflow.openResourcesScreen()
                        windowContext.depricatedUiController.layout = null
                        control.close()
                    }
                }
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

    onClosed:
    {
        searchEdit.clear()
    }

    onAboutToShow:
    {
        treeView.expandDefault()
    }

    ResourceTreeModel { id: treeModel }

    ResourceTreeSearchModel
    {
        id: searchModel
        sourceModel: treeModel
        filterRegularExpression: searchEdit.regExp
    }
}
