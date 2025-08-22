// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Rectangle
{
    color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.5)
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
