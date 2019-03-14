import QtQuick 2.11
import Nx 1.0

Text
{
    id: menuItem

    property NavigationMenu navigationMenu: null
    property var itemId: this
    readonly property bool current: navigationMenu && navigationMenu.currentItemId === itemId
    property bool active: true

    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13
    font.weight: Font.Medium
    width: parent.width
    height: 24
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignLeft
    elide: Text.ElideRight

    signal clicked()

    MouseArea
    {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked:
        {
            if (navigationMenu)
                navigationMenu.currentItemId = itemId
            menuItem.clicked()
        }
    }

    Rectangle
    {
        anchors.fill: parent
        z: -1

        color:
        {
            if (!menuItem.current && !mouseArea.containsMouse)
                return "transparent"

            const color = menuItem.current ? ColorTheme.highlight : ColorTheme.colors.dark12
            const opacity = mouseArea.containsMouse ? 0.5 : 0.4
            return ColorTheme.transparent(color, opacity)
        }
    }

    color:
    {
        if (menuItem.current)
            return ColorTheme.text

        if (!menuItem.active || !menuItem.enabled)
            return ColorTheme.colors.dark14

        return ColorTheme.light
    }
}
