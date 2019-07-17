import QtQml.Models 2.11
import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx 1.0

TabBar
{
    id: tabBar

    property alias bottomBorderColor: tabBar.palette.dark

    topPadding: 0
    bottomPadding: 0
    leftPadding: 8
    rightPadding: 8
    spacing: 8

    height: implicitHeight

    palette
    {
        buttonText: ColorTheme.colors.light12
        brightText: ColorTheme.colors.light10
        highlightedText: ColorTheme.colors.brand_core
        dark: ColorTheme.colors.dark10
        mid: ColorTheme.colors.dark12
    }

    background: Item
    {
        Rectangle
        {
            id: border

            color: palette.dark
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
        }
    }
}
