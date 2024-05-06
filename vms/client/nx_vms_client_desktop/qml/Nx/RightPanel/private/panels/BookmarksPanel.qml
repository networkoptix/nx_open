// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.RightPanel

import nx.vms.client.desktop

SearchPanel
{
    id: bookmarksPanel

    type: { return RightPanelModel.Type.bookmarks }

    placeholder
    {
        icon: "image://skin/left_panel/placeholders/bookmarks.svg"
        title: qsTr("No bookmarks")
        description: qsTr("Select a time span on the timeline and right-click the "
            + "highlighted section to create a bookmark")
    }
}
