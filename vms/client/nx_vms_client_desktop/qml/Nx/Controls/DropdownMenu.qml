// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

Menu
{
    id: menu

    implicitWidth: contentItem.childrenRect.width + leftPadding + rightPadding

    delegate: CompactMenuItem
    {
        id: menuItem

        rightPadding: 28

        textColor: action?.checked ? ColorTheme.colors.brand_contrast : ColorTheme.colors.light4

        background: Rectangle
        {
            color:
            {
                if (action?.checked)
                    return ColorTheme.colors.brand_core

                if (menuItem.pressed)
                    return ColorTheme.lighter(ColorTheme.colors.dark13, 1)

                return menuItem.hovered || menuItem.activeFocus
                    ? ColorTheme.lighter(ColorTheme.colors.dark13, 2)
                    : ColorTheme.colors.dark13
            }
        }
    }
}
