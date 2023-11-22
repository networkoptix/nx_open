// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

Text
{
    id: menuItem

    property NavigationMenu navigationMenu: null
    property var itemId: this
    readonly property bool current: navigationMenu && navigationMenu.currentItemId === itemId
    property bool active: true

    leftPadding: 16
    rightPadding: 16
    font.pixelSize: FontConfig.normal.pixelSize
    width: parent.width
    height: 24
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignLeft
    elide: Text.ElideRight

    function click()
    {
        if (navigationMenu)
        {
            navigationMenu.currentItemId = itemId
            navigationMenu.itemClicked(menuItem)
        }

        clicked()
    }

    signal clicked()

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked:
            menuItem.click()
    }

    Rectangle
    {
        anchors.fill: parent
        z: -1

        color:
        {
            if (!menuItem.current && !mouseArea.containsMouse)
                return "transparent"

            const color = menuItem.current ? ColorTheme.highlight : ColorTheme.colors.dark12
            const opacity = mouseArea.containsMouse ? 0.5 : 0.4
            return ColorTheme.transparent(color, opacity)
        }
    }

    color:
    {
        if (menuItem.current)
            return ColorTheme.text

        if (!menuItem.active || !menuItem.enabled)
            return ColorTheme.colors.dark14

        return ColorTheme.light
    }
}
