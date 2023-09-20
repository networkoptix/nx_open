// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

QtObject
{
    property color normal: ColorTheme.colors.light10
    property color hovered: ColorTheme.lighter(normal, 2)
    property color pressed: hovered
}
