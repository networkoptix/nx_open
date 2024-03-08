// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Menu
{
    id: menu

    modal: !parentMenu //< Sub-menus MUST be modeless.
    Overlay.modeless: Item {}
    Overlay.modal: Item {}

    topPadding: 2
    bottomPadding: 2

    overlap: -1

    width: Math.max(implicitItemsWidth + leftPadding + rightPadding,
        background ? background.implicitWidth : 0)

    readonly property var parentMenu: ((parent instanceof MenuItem) && parent.menu) || null

    readonly property real implicitItemsWidth:
    {
        let result = 0

        for (let i = 0; i < count; ++i)
            result = Math.max(result, itemAt(i).implicitWidth)

        return result
    }

    /** Whether to translate `triggered` signal to the parent menu automatically. */
    property bool notifiesParentMenu: true

    signal triggered(Action action, MenuItem item)

    onTriggered: (action, item) =>
        notifiesParentMenu && parentMenu && parentMenu.triggered(action, item)

    delegate: MenuItem {}

    contentItem: Column
    {
        Repeater
        {
            model: menu.contentModel
        }
    }

    background: Item
    {
        Rectangle
        {
            id: backgroundRect

            radius: 2
            color: ColorTheme.midlight
            anchors.fill: parent
        }

        PopupShadow
        {
            source: backgroundRect
            visible: true
        }
    }

    Connections
    {
        target: menu.parent
        enabled: target && menu.visible

        function onVisibleChanged()
        {
            if (!target.visible)
                menu.dismiss()
        }
    }

    Connections
    {
        target: menu.parent && menu.parent.Window.window
        enabled: target && menu.visible

        function onVisibleChanged()
        {
            if (!target.visible)
                menu.dismiss()
        }
    }

    MouseSpy.onMousePress: (mouse) =>
    {
        if (!menu.visible)
            return

        const overlay = menu.Overlay.overlay
        const pt = overlay.mapFromGlobal(mouse.globalPosition)

        if (pt.x < 0 || pt.y < 0 || pt.x >= overlay.width || pt.y >= overlay.height)
            menu.dismiss()
    }
}
