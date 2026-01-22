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

    readonly property bool isNothingFound: searchEdit.regExp && treeView.rows === 0

    ColumnLayout
    {
        anchors.fill: parent

        SearchEdit
        {
            id: searchEdit

            Layout.fillWidth: true
            height: 36

            Layout.leftMargin: 16
            Layout.rightMargin: 16

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
