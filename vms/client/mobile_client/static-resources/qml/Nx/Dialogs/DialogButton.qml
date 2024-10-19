// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0
import Nx.Controls 1.0

Button
{
    id: control

    property bool accented: false

    flat: true
    textColor: accented
        ? ColorTheme.colors.brand_core
        : ColorTheme.colors.light10
    highlightColor: ColorTheme.colors.dark6
    font.weight: Font.Normal

    implicitHeight: 48
    radius: 0

    padding: 0
    leftPadding: 0
    rightPadding: 0
    opacity: enabled ? 1.0 : 0.3
}
