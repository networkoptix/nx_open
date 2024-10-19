// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

Text
{
    text: qsTr("You are not connected to any Site")
    verticalAlignment: Text.AlignVCenter
    horizontalAlignment: Text.AlignHCenter
    lineHeight: 20
    lineHeightMode: Text.FixedHeight
    wrapMode: Text.WordWrap
    color: ColorTheme.colors.dark16
    font.pixelSize: 16
}
