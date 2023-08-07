// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Effects

import Nx.Core

MultiEffect
{
    source: null

    z: -1

    autoPaddingEnabled: true
    shadowEnabled: true
    shadowColor: ColorTheme.colors.dark4
    shadowOpacity: 0.5
    shadowVerticalOffset: 10

    blurMax: 50

    width: source ? source.width : 0
    height: source ? source.height : 0
}
