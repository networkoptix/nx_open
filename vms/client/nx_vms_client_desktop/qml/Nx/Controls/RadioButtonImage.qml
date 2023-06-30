// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick.Controls.impl

import Nx.Core
import Nx.Core.Controls

import "private"

IconImage
{
    property bool checked: false
    property bool hovered: false
    property bool pressed: false
    property bool enabled: true

    readonly property ButtonColors colors: ButtonColors { normal: ColorTheme.colors.light10 }
    readonly property ButtonColors checkedColors: ButtonColors { normal: ColorTheme.colors.light4 }

    opacity: enabled ? 1.0 : 0.3
    baselineOffset: 12
    sourceSize: Qt.size(16, 16)

    color:
    {
        const baseColors = checked ? checkedColors : colors
        if (!enabled)
            return baseColors.normal

        return pressed
            ? baseColors.pressed
            : (hovered ? baseColors.hovered : baseColors.normal)
    }

    source:
    {
        return checked
            ? "qrc:///skin/theme/radiobutton_checked.png"
            : "qrc:///skin/theme/radiobutton.png"
    }
}
