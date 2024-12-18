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
        visible: icon != ""
        onClicked: leftButtonClicked()
        alwaysCompleteHighlightAnimation: false
    }

    PageTitleLabel
    {
        id: label
        anchors.verticalCenter: parent.verticalCenter
        x: leftButton.visible ? 72 : 16
        width: controlsRow.x - x - 8
    }

    Row
    {
        id: controlsRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 8
    }
}
