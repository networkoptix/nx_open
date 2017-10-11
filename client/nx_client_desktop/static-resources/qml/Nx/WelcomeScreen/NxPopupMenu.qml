import QtQuick 2.6;
import Qt.labs.controls 1.0 as Labs;
import Nx.WelcomeScreen 1.0;

Labs.Menu
{
    id: control;

    topPadding: 2;
    bottomPadding: 2;
    background: Rectangle
    {
        implicitHeight: 300;

        radius: 2;
        color: Style.menu.background;
    }

    Binding
    {
        target: contentItem
        property: "implicitWidth"
        value: contentItem.contentItem.childrenRect.width;
    }
}
