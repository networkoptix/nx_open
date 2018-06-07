import QtQuick 2.6
import QtQuick.Controls 2.4 as Labs
import Nx 1.0

Labs.Menu
{
    id: control;

    topPadding: 2;
    bottomPadding: 2;
    background: Rectangle
    {
        implicitHeight: 300;

        radius: 2;
        color: ColorTheme.midlight;
    }

    Binding
    {
        target: contentItem
        property: "implicitWidth"
        value: contentItem.contentItem.childrenRect.width;
    }
}
