// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml

import Nx.Core
import Nx.Core.Controls

import "private"

IconImage
{
    property int checkState: Qt.Unchecked
    property bool hovered: false
    property bool pressed: false
    property bool enabled: true

    readonly property ButtonColors colors: ButtonColors { normal: ColorTheme.colors.light10 }
    readonly property ButtonColors checkedColors: ButtonColors { normal: ColorTheme.colors.light4 }

    opacity: enabled ? 1.0 : 0.3
    baselineOffset: 14
    sourceSize: Qt.size(20, 20)

    color:
    {
        const baseColors = (checkState === Qt.Unchecked) ? colors : checkedColors
        if (!enabled)
            return baseColors.normal

        return pressed
            ? baseColors.pressed
            : (hovered ? baseColors.hovered : baseColors.normal)
    }

    source:
    {
        switch (checkState)
        {
            case Qt.Checked:
                return "image://svg/skin/theme/checkbox_checked.svg"

            case Qt.Unchecked:
                return "image://svg/skin/theme/checkbox_unchecked.svg"

            case Qt.PartiallyChecked:
                return "image://svg/skin/theme/checkbox_partially_checked.svg"

            default:
                console.assert(false, `Invalid check state ${checkState}`)
                return null
        }
    }
}
