// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQml
import QtQuick

import Nx.Core

import nx.vms.client.mobile

QtObject
{
    id: styleHints

    // Common height for the toolbar and panel header.
    readonly property int headerHeight: 64

    // Name of the color of the texts and icons of the main controls: toolbar title and buttons,
    // navigation bar buttons, panel header title and buttons.
    readonly property string foregroundColorName: "light4"
    readonly property color foregroundColor: ColorTheme.colors[styleHints.foregroundColorName]

    // Width of the side panels on tablet layout.
    readonly property int panelWidth: appContext.settings.sidePanelWidth

    // Width of the sheet on tablet layout. By design is unified with the side panel width.
    readonly property int sheetWidth: panelWidth

    // Preferred sheet edge in landscape. Left-handed mode is not supported on tablet layout.
    readonly property int preferredSheetEdge:
        (!LayoutController.isTabletLayout && appContext.settings.leftHandedMode)
            ? Qt.LeftEdge
            : Qt.RightEdge

    // Minimum width of the content area between the side panels on tablet layout. If a panel
    // would push the content area below this threshold, the auto-close logic in AdaptiveScreen
    // hides one of the panels.
    readonly property int contentAreaMinimumWidth: appContext.settings.contentAreaMinWidth

    // Preferred preview height.
    readonly property int previewHeight: 270
}
