import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx 1.0

Control
{
    id: tile

    palette
    {
        base: model.isInformer ? ColorTheme.colors.dark8 : ColorTheme.colors.dark7
        midlight: model.isInformer ? ColorTheme.colors.dark9 : ColorTheme.colors.dark8
        light: ColorTheme.colors.light10
        windowText: ColorTheme.colors.light16
        text: ColorTheme.colors.light4
        highlight: ColorTheme.colors.brand_core
        shadow: ColorTheme.colors.dark5
        dark: ColorTheme.colors.dark6
    }

    leftPadding: 8
    topPadding: 8
    rightPadding: 8
    bottomPadding: 8

    background: Rectangle
    {
        color: tile.hovered ? tile.palette.midlight : tile.palette.base
        radius: 2
    }
}
