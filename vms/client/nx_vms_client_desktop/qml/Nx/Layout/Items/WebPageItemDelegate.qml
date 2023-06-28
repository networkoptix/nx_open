// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.leftContent.children:
    [
        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/back.svg"
        },

        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/refresh.svg"
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            iconUrl: "image://svg/skin/item/fullscreen.svg"
            checkedIconUrl: "image://svg/skin/item/exit_fullscreen.svg"
            checkable: true
        },

        TitleBarButton
        {
            id: infoButton

            iconUrl: "image://svg/skin/item/info.svg"
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
            iconUrl: "image://svg/skin/item/close.svg"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}
