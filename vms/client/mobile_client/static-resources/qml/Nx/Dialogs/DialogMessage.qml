// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

Text
{
    width: parent ? parent.width : implicitWidth
    height: contentHeight + topPadding + bottomPadding
    padding: 16
    topPadding: 0
    font.pixelSize: 16
    wrapMode: Text.WordWrap
    color: ColorTheme.colors.light4
    visible: !!text
}
