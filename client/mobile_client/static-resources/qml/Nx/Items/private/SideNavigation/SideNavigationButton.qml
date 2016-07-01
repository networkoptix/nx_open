import QtQuick 2.0
import Nx 1.0

SideNavigationItem
{
    id: control

    property string icon: undefined
    property string text: undefined

    contentItem: Item
    {
        Image
        {
            anchors.verticalCenter: parent.verticalCenter
            x: 16
            source: control.icon
        }

        Text
        {
            x: 56
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: ColorTheme.windowText
        }
    }
}
