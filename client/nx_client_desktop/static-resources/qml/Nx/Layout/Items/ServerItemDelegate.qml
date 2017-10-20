import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            icon: "qrc:/skin/item/issues.png"
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/log.png"
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
