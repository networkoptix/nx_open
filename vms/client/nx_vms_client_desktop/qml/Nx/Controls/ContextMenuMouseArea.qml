// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Controls 1.0

MouseArea
{
    property Component menu: null
    property int parentSelectionStart: 0
    property int parentSelectionEnd: 0

    signal menuOpened(int prevSelectionStart, int prevSelectionEnd)

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

        var selectionStart = parentSelectionStart
        var selectionEnd = parentSelectionEnd

        menuLoader.item.x = mouse.x
        menuLoader.item.y = mouse.y
        menuLoader.item.selectionActionsEnabled = selectionStart !== selectionEnd
        menuLoader.item.open()

        menuOpened(selectionStart, selectionEnd)
    }
}
