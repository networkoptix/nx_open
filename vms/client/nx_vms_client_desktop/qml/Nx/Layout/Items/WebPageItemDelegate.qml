// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.leftContent.children:
    [
        TitleBarButton
        {
            icon.source: "image://skin/item/back.svg"
        },

        TitleBarButton
        {
            icon.source: "image://skin/item/refresh.svg"
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            icon.source: checked
                ? "image://skin/item/exit_fullscreen.svg"
                : "image://skin/item/fullscreen.svg"

            checkable: true
        },

        TitleBarButton
        {
            id: infoButton

            icon.source: "image://skin/item/info.svg"
            checkable: true

            Binding
            {
                target: infoButton
                property: "checked"
                value: layoutItemData.displayInfo
            }
            // Use Action from Qt 5.10
            onCheckedChanged: layoutItemData.displayInfo = checked
        },

        TitleBarButton
        {
            icon.source: "image://skin/item/close.svg"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}
