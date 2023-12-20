// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Core
import Nx.Controls.NavigationMenu

MenuItem
{
    property int level: 0
    property bool selected: false
    property alias iconComponent: icon.sourceComponent

    font.pixelSize: 14

    height: 28
    leftPadding: icon.x + icon.implicitWidth + 4
    color:
    {
        const color = current || (selected && level === 0)
            ? ColorTheme.colors.light1
            : ColorTheme.colors.light10
        const opacity = active ? 1.0 : 0.3
        return ColorTheme.transparent(color, opacity)
    }

    Loader
    {
        id: icon
        x: (level + 1) * 8
        anchors.verticalCenter: parent.verticalCenter
    }
}
