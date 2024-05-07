// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import "../globals.js" as RightPanelGlobals
import nx.vms.client.desktop.analytics as Analytics

QtObject
{
    property bool showInformation: true
    property bool showThumbnails: true
    property bool showIcons: true
    property int selectedRow: -1

    property var selectedTile: null
    property var hoveredTile: null

    property int videoPreviewMode: RightPanelGlobals.VideoPreviewMode.hover
    property Analytics.AttributeDisplayManager attributeManager: null

    signal clicked(int row, int mouseButton, int keyboardModifiers)
    signal doubleClicked(int row)
    signal dragStarted(int row, point pos, size size)
    signal linkActivated(int row, string link)
    signal contextMenuRequested(int row, point globalPos)
    signal closeRequested(int row)
    signal hoverChanged(int row, bool hovered)
}
