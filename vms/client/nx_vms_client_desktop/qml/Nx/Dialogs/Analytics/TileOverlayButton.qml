// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

ImageButton
{
    property bool accent: false

    width: 24
    height: 24

    backgroundColor:
    {
        const base = accent ? ColorTheme.colors.brand_core : ColorTheme.colors.dark1
        const alpha = down ? 0.5 : (hovered ? 0.9 : 0.7)
        return ColorTheme.transparent(base, alpha)
    }
}
