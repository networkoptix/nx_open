// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

Popup
{
    id: control

    default property alias data: contentColumn.data

    parent: mainWindow.contentItem

    y: mainWindow.height - height - d.keyboardHeight
    implicitWidth: mainWindow.width
    implicitHeight: Math.min(
        flickable.contentHeight + control.topPadding + control.bottomPadding,
        mainWindow.availableHeight - d.keyboardHeight)

    modal: true
    padding: 20
    topPadding: 22
    bottomPadding: 32

    background: Item
    {
        Rectangle
        {
            id: roundPartBackground

            anchors.top: parent.top
            width: parent.width
            height: 2 * radius
            radius: 10
            color: ColorTheme.colors.dark9
        }

        Rectangle
        {
            id: mainAreaBackground

            y: 10
            width: parent.width
            height: parent.height - y
            color: roundPartBackground.color
        }

        Rectangle
        {
            id: lineSeparator

            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 3
            y: 8

            color: ColorTheme.colors.dark15
        }
    }

    contentItem: Flickable
    {
        id: flickable

        clip: true
        contentHeight: contentColumn.height

        Column
        {
            id: contentColumn

            spacing: control.spacing
            width: parent.width
        }
    }

    NxObject
    {
        id: d

        // Temporary solution. Will be fixed properly after UI refactor.
        readonly property real keyboardHeight: Qt.platform.os === "android"
            ? androidKeyboardHeight
            : 0

    }
}
