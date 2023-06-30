// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls

import nx.vms.client.core

Menu
{
    id: menu

    property bool readActionsVisible: true
    property bool selectionActionsEnabled: true
    property bool editingEnabled: true

    signal cutAction
    signal copyAction
    signal pasteAction
    signal deleteAction
    signal itemActiveFocusChanged(bool itemActiveFocus)

    Component.onCompleted:
    {
        for (let i = 0; i < count; ++i)
            itemAt(i).activeFocusChanged.connect(itemActiveFocusChanged);
    }

    Action
    {
        text: "Cut"
        shortcut: StandardKey.Cut
        onTriggered: menu.cutAction()
        visible: menu.readActionsVisible && menu.editingEnabled
        enabled: menu.selectionActionsEnabled && menu.editingEnabled && menu.visible
    }

    Action
    {
        text: "Copy"
        shortcut: StandardKey.Copy
        onTriggered: menu.copyAction()
        visible: menu.readActionsVisible
        enabled: menu.selectionActionsEnabled && menu.visible
    }

    Action
    {
        text: "Paste"
        shortcut: StandardKey.Paste
        onTriggered: menu.pasteAction()
        visible: menu.editingEnabled
        enabled: menu.editingEnabled && menu.visible
    }

    Action
    {
        text: "Delete"
        onTriggered: menu.deleteAction()
        visible: menu.editingEnabled
        enabled: menu.selectionActionsEnabled && menu.editingEnabled && menu.visible
    }
}
