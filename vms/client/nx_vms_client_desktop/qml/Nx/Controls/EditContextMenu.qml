// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Controls 1.0
import nx.vms.client.core 1.0

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
        for (var i = 0; i < count; ++i)
            itemAt(i).activeFocusChanged.connect(itemActiveFocusChanged);
    }

    Action
    {
        text: "Cut"
        shortcut: "Ctrl+X"
        onTriggered: menu.cutAction()
        visible: menu.readActionsVisible && menu.editingEnabled
        enabled: menu.selectionActionsEnabled && menu.editingEnabled
    }

    Action
    {
        text: "Copy"
        shortcut: "Ctrl+C"
        onTriggered: menu.copyAction()
        visible: menu.readActionsVisible
        enabled: menu.selectionActionsEnabled
    }

    Action
    {
        text: "Paste"
        shortcut: "Ctrl+V"
        onTriggered: menu.pasteAction()
        visible: menu.editingEnabled
    }

    Action
    {
        text: "Delete"
        onTriggered: menu.deleteAction()
        visible: menu.editingEnabled
        enabled: menu.selectionActionsEnabled && menu.editingEnabled
    }
}
