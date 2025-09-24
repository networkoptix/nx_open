// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

import Nx.Controls 1.0
import Nx.Core 1.0

Button
{
    property bool accented: false

    implicitWidth: parent ? parent.width : 100
    font.pixelSize: 16
    font.weight: Font.Medium
    color: accented ? ColorTheme.colors.brand_core : ColorTheme.colors.dark11
    textColor: accented ? ColorTheme.colors.brand_contrast : ColorTheme.colors.light10
    flat: !accented

    implicitHeight: 44
    padding: 0
    leftPadding: 0
    rightPadding: 0
}
