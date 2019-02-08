import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0

ScrollView
{
    id: navigationMenu

    property var currentItemId: null

    default property alias data: column.data

    topPadding: 16
    bottomPadding: 16

    background: Rectangle
    {
        color: ColorTheme.colors.dark8

        Rectangle
        {
            x: parent.width
            width: 1
            height: parent.height
            color: ColorTheme.shadow
        }
    }

    Flickable
    {
        boundsBehavior: Flickable.StopAtBounds

        Column
        {
            id: column
            width: navigationMenu.width

            onChildrenChanged:
            {
                for (var i = 0; i < children.length; ++i)
                {
                    var item = children[i]
                    if (item.hasOwnProperty("navigationMenu"))
                        item.navigationMenu = navigationMenu
                }
            }
        }
    }
}
