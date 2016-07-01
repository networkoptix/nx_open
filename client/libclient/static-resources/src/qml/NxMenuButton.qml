import QtQuick 2.6;

NxImageButton
{
    id: control;

    property NxPopupMenu menu;

    visible: menu;

    forcePressed: (menu && menu.visible);
    onClicked: menu.open();

    standardIconUrl: "qrc:/skin/welcome_page/drop_menu.png";
    hoveredIconUrl: "qrc:/skin/welcome_page/drop_menu_hovered.png";
    pressedIconUrl: "qrc:/skin/welcome_page/drop_menu_pressed.png";

    onMenuChanged:
    {
        if (!control.menu)
            return;

        menu.parent = control;
        updateMenuPosition();
    }

    onWidthChanged: { updateMenuPosition(); }
    onHeightChanged: { updateMenuPosition(); }

    function updateMenuPosition()
    {
        if (menu)
            menu.y = height;
    }
}
