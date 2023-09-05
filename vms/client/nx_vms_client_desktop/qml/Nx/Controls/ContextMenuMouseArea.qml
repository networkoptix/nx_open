// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls

import nx.vms.client.desktop

MouseArea
{
    id: mouserArea

    property Component menu: null
    property int parentSelectionStart: 0
    property int parentSelectionEnd: 0
    property int parentCursorPosition: -1
    property alias menuItem: menuLoader.item

    signal menuAboutToBeOpened()
    signal menuOpened(int prevSelectionStart, int prevSelectionEnd, int cursorPosition)

    acceptedButtons: Qt.RightButton

    Loader
    {
        id: menuLoader
    }

    onClicked:
    {
        if (!menuLoader.item)
        {
            if (menu)
                menuLoader.sourceComponent = menu
            else
                return
        }

        menuAboutToBeOpened()

        const selectionStart = parentSelectionStart
        const selectionEnd = parentSelectionEnd
        const cursorPosition = parentCursorPosition

        menuLoader.item.x = mouse.x
        menuLoader.item.y = mouse.y
        menuLoader.item.selectionActionsEnabled = selectionStart !== selectionEnd
        menuLoader.item.open()

        menuOpened(selectionStart, selectionEnd, cursorPosition)
    }
}
