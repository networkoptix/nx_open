// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

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
