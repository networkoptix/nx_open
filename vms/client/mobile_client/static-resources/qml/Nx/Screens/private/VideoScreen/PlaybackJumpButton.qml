// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Controls 1.0
import Nx.Core 1.0

IconButton
{
    property bool forward: true

    backgroundColor: ColorTheme.transparent(ColorTheme.colors.dark1, 0.2)
    disabledBackgroundColor: ColorTheme.transparent(ColorTheme.colors.dark1, 0.1)

    icon.source: forward ? lp("/images/playback_fwd.svg") : lp("/images/playback_rwd.svg")
    icon.width: 24
    icon.height: icon.height
    padding: 8
    width: 56
    height: width
}
