import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.leftContent.children:
    [
        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/back.png"
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/refresh.png"
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/fullscreen.png"
            checkedIconUrl: "qrc:/skin/item/exit_fullscreen.png"
            checkable: true
        },

        TitleBarButton
        {
            id: infoButton

            iconUrl: "qrc:/skin/item/info.png"
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
            iconUrl: "qrc:/skin/item/close.png"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}
