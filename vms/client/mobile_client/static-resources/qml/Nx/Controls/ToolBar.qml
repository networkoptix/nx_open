// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Nx.Controls 1.0

import "."

ToolBarBase
{
    id: toolBar

    property alias leftButtonIcon: leftButton.icon
    property alias title: label.text
    property alias controls: controlsRow.data
    property alias titleOpacity: label.opacity

    signal leftButtonClicked()

    IconButton
    {
        id: leftButton
        anchors.verticalCenter: parent.verticalCenter
        x: 8
        padding: 0
        visible: icon.source != ""
        onClicked: leftButtonClicked()
        alwaysCompleteHighlightAnimation: false
    }

    PageTitleLabel
    {
        id: label

        readonly property real buttonSpace:
            8 + (leftButton.width ? leftButton.x + leftButton.width : 0)
        readonly property real maxWidth: parent.width - buttonSpace - (controlsRow.width + 8)

        x: Math.max(parent.width / 2 - width / 2, buttonSpace)
        width: Math.max(0, Math.min(implicitWidth, maxWidth))
        anchors.verticalCenter: parent.verticalCenter
    }

    Row
    {
        id: controlsRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
    }
}
