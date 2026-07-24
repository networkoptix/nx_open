// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQml

import nx.vms.client.mobile

QtObject
{
    // Common height for the toolbar and panel header.
    readonly property int headerHeight: 64

    // Width of the side panels on tablet layout.
    readonly property int panelWidth: appContext.settings.sidePanelWidth

    // Width of the sheet on tablet layout. By design is unified with the side panel width.
    readonly property int sheetWidth: panelWidth

    // Minimum width of the content area between the side panels on tablet layout. If a panel
    // would push the content area below this threshold, the auto-close logic in AdaptiveScreen
    // hides one of the panels.
    readonly property int contentAreaMinimumWidth: appContext.settings.contentAreaMinWidth

    // Preferred preview height depending on the current device layout.
    readonly property int previewHeight:
    {
        if (LayoutController.fullscreen)
            return -1 //< Must fill all the available space.

        if (LayoutController.isTablet && LayoutController.isPortrait)
            return 536

        // Preview has the same height on a mobile device in portrait orientation and on the right
        // panel in tablet layout.
        return 270
    }
}
