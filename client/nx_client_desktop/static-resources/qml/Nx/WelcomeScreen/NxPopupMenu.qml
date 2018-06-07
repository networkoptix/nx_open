import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

Menu
{
    id: control

    topPadding: 2
    bottomPadding: 2

    background: Rectangle
    {
        radius: 2
        color: ColorTheme.midlight
    }

    Binding
    {
        target: contentItem
        property: "implicitWidth"
        value: contentItem.contentItem.childrenRect.width
    }
}
