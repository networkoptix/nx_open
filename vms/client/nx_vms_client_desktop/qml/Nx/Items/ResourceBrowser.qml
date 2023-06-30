// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.RightPanel 1.0

import "private"

Item
{
    id: resourcesPanel

    property alias scene: resourceTree.scene
    property alias tree: resourceTree

    clip: true

    function clearFilters()
    {
        resourceSearchPane.clear()
    }

    function focusSearchField()
    {
        resourceSearchPane.focus = true
    }

    ResourceSearchPane
    {
        id: resourceSearchPane

        width: parent.width - x - 1
        menuEnabled: !resourceTree.localFilesMode
        isFilterRelevant: (type => resourceTree.isFilterRelevant(type))

        onEnterPressed: (modifiers) =>
        {
            if (resourceTree.model.isFiltering)
                resourceTree.model.activateSearchResults(modifiers)
        }

        Keys.onPressed: (event) =>
        {
            if (event.key === Qt.Key_Down)
            {
                resourceTree.focus = true
                resourceTree.setSelection([resourceTree.topmostSelectableIndex()])
            }

            event.accepted = true
        }
    }

    ResourceTree
    {
        id: resourceTree

        y: resourceSearchPane.height
        width: parent.width - 1
        height: parent.height - y

        filterText: resourceSearchPane.filterText
        filterType: resourceSearchPane.filterType
        shortcutHintsEnabled: resourceSearchPane.activeFocus

        toolTipEnclosingRect: Qt.rect(
            resourceTree.width,
            -resourceSearchPane.height + 2,
            Number.MAX_VALUE / 2,
            resourceTree.height + resourceSearchPane.height - 4)

        function topmostSelectableIndex()
        {
            const count = resourceTree.model.rowCount(resourceTree.rootIndex)
            for (let row = 0; row < count; ++row)
            {
                const index = resourceTree.model.index(row, 0, resourceTree.rootIndex)
                if (resourceTree.model.flags(index) & Qt.ItemIsSelectable)
                    return index
            }

            return NxGlobals.invalidModelIndex()
        }

        Keys.onUpPressed: (event) =>
        {
            if (NxGlobals.fromPersistent(currentIndex) == topmostSelectableIndex())
                resourceSearchPane.focus = true
            else
                event.accepted = false
        }

        Connections
        {
            target: resourceTree.model

            function onLocalFilesModeChanged()
            {
                resourcesPanel.clearFilters()
            }
        }
    }

    ResultsPlaceholder
    {
        id: placeholderItem

        anchors.topMargin: resourceTree.y
        anchors.fill: resourceBrowser
        shown: resourceTree.empty

        readonly property bool localFilesAbsent:
            resourceTree.localFilesMode && !resourceTree.isFiltering

        icon: localFilesAbsent
            ? "image://svg/skin/left_panel/placeholders/folder.svg"
            : "image://svg/skin/left_panel/placeholders/search.svg"

        title: parent.localFilesAbsent ? qsTr("No local files") : qsTr("Nothing found")

        description:
        {
            if (parent.localFilesAbsent)
            {
                return qsTr("Drag video files or images to the client window, or add local media"
                    + " folder through the Local Settings dialog")
            }

            return qsTr("Try searching for something else")
        }
    }
}
