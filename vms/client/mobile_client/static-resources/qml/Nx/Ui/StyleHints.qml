// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQml

import nx.vms.client.mobile

QtObject
{
    // Common height for the toolbar and panel header.
    readonly property int headerHeight: 64

    // Width of the side panels on tablet layout.
    property int panelWidth: appContext.settings.sidePanelWidth

    // Minimum width of the content area between the side panels on tablet layout. If a panel
    // would push the content area below this threshold, the auto-close logic in AdaptiveScreen
    // hides one of the panels.
    readonly property int contentAreaMinimumWidth: 560
}
