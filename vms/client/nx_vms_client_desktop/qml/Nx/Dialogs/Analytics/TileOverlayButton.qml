// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

ImageButton
{
    property bool accent: false

    icon.color: "transparent"

    width: 24
    height: 24
    radius: 2

    property var backgroundColor: accent
        ? ColorTheme.colors.brand_core
        : ColorTheme.colors.dark1

    normalBackground: ColorTheme.transparent(backgroundColor, 0.7)
    hoveredBackground: ColorTheme.transparent(backgroundColor, 0.9)
    pressedBackground: ColorTheme.transparent(backgroundColor, 0.5)

    hoverEnabled: true
}
