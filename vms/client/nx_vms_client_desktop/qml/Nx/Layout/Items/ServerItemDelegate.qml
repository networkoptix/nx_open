import QtQuick 2.6

ResourceItemDelegate
{
    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/issues.png"
        },

        TitleBarButton
        {
            iconUrl: "qrc:/skin/item/log.png"
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
