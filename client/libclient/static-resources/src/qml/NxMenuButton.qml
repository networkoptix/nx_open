import QtQuick 2.6;

NxImageButton
{
    id: control;

    property NxPopupMenu menu;

    visible: menu;

    onClicked:
    {
        if (!menu)
            return;

        forcePressed = true;
        prevMenuParent = menu.parent;
        menu.parent = control;
        menu.y = height;
        menu.x = 0;
        menu.open();
    }

    Connections
    {
        target: control.menu;
        onVisibleChanged:
        {
            if (menu.visible || !forcePressed)
                return;

            menu.parent = prevMenuParent;
            forcePressed = false;
        }
    }

    standardIconUrl: "qrc:/skin/welcome_page/drop_menu.png";
    hoveredIconUrl: "qrc:/skin/welcome_page/drop_menu_hovered.png";
    pressedIconUrl: "qrc:/skin/welcome_page/drop_menu_pressed.png";

    property variant prevMenuParent: null;
}
