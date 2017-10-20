import QtQuick 2.6

ResourceItemDelegate
{
    rotationAllowed: true

    titleBar.rightContent.children:
    [
        TitleBarButton
        {
            icon: "qrc:/skin/item/image_enhancement.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/zoom_window.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/ptz.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/fisheye.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/search.png"
            checkable: true
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/screenshot.png"
        },

        TitleBarButton
        {
            icon: "qrc:/skin/item/rotate.png"

            onPressed: resourceItem.rotationInstrument.start(mousePosition, this)
            onMousePositionChanged: resourceItem.rotationInstrument.move(mousePosition, this)
            onReleased: resourceItem.rotationInstrument.stop()
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
