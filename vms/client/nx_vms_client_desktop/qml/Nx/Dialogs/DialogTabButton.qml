// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Controls 1.0

CompactTabButton
{
    height: parent ? parent.height : 0

    underlineOffset: -1
    topPadding: 14
    focusFrameEnabled: false

    compact: false

    underlineHeight: 2
    inactiveUnderlineColor: "transparent"
    font: Qt.font({pixelSize: 12, weight: Font.Normal})
}
