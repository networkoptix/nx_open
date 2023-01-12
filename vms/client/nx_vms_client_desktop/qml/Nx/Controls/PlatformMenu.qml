// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Qt.labs.platform 1.1

Menu
{
    id: menu

    readonly property alias shown: d.shown

    signal triggered(Action action)

    Component.onCompleted:
    {
        let currentIndex = 0
        let creationInfo = []

        for (let i = 0; i < data.length; ++i)
        {
            const item = data[i]
            if (item instanceof Action)
                creationInfo.push({"action": item, "index": currentIndex++})
            else if ((item instanceof Menu) || (item instanceof MenuItem))
                ++currentIndex
            else if (item instanceof Item)
                console.warn(`A visual item ${item} placed in a PlatformMenu object`)
        }

        for (let i = 0; i < creationInfo.length; ++i)
        {
            const info = creationInfo[i]
            menu.insertItem(info.index, d.delegate.createObject(d, {"action": info.action}))
            info.action.triggered.connect(function() { menu.triggered(info.action) })
        }
    }

    QtObject
    {
        id: d
        readonly property Component delegate: PlatformMenuItem {}
        property bool shown: false
    }

    onTriggered: (action) =>
    {
        if (parentMenu && (parentMenu instanceof PlatformMenu))
            parentMenu.triggered(action)
    }

    onAboutToShow:
        d.shown = true

    onAboutToHide:
        d.shown = false
}
