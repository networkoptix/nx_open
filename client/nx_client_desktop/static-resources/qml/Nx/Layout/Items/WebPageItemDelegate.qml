import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.leftContent.children:
    [
        TitleBarButton
        {
            icon: "qrc:/skin/item/back.png"
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/refresh.png"
        }
    ]

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            icon: "qrc:/skin/item/fullscreen.png"
            checkedIcon: "qrc:/skin/item/exit_fullscreen.png"
            checkable: true
        },

        TitleBarButton
        {
            id: infoButton

            icon: "qrc:/skin/item/info.png"
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
            icon: "qrc:/skin/item/close.png"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}
