// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Controls 1.0

Button
{
    id: menuButton

    property var menuComponent: null

    // It needed for eliminating closing menu's mouse area. It catches signals of its menuButton.
    property var prevMenuParent: null

    signal menuClosed()

    function closeMenu()
    {
        if (menuLoader.item)
            menuLoader.item.close()
    }

    width: 19
    height: 23

    flat: true
    backgroundColor: "transparent"
    pressedColor: ColorTheme.colors.dark6
    hoveredColor: ColorTheme.transparent(ColorTheme.colors.light10, 0.05)
    outlineColor: "transparent"

    focusPolicy: Qt.NoFocus

    onClicked:
    {
        if (!menuLoader.item)
        {
            menuLoader.sourceComponent = menuComponent

            menuLoader.x = 0
            menuLoader.y = height
            menuLoader.item.open()
        }
        else
        {
            if (!menuLoader.item.visible)
                menuLoader.item.parent = menuButton.prevMenuParent

            menuLoader.item.visible = !menuLoader.item.visible
        }
    }

    Connections
    {
        target: menuLoader.item

        function onVisibleChanged()
        {
            if (menuLoader.item.visible)
                return

            menuButton.prevMenuParent = menuLoader.item.parent
            menuLoader.item.parent = null
        }
    }

    Loader
    {
        id: menuLoader
    }

    Connections
    {
        target: menuLoader.item

        function onClosed()
        {
            menuButton.menuClosed()
        }
    }

    Image
    {
        anchors.centerIn: parent
        width: 3
        height: 15

        source: "image://svg/skin/welcome_screen/tile/dots.svg"
        sourceSize: Qt.size(width, height)
    }
}
