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
            icon: "qrc:/skin/item/info.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/close.png"
            onClicked: layoutItemData.layout.removeItem(layoutItemData.itemId)
        }
    ]
}
