// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml
import QtQuick.Controls.impl 2.15

import Nx 1.0

IconImage
{
    property int checkState: Qt.Unchecked
    property bool hovered: false
    property bool enabled: true

    opacity: enabled ? 1.0 : 0.3
    baselineOffset: 15
    sourceSize: Qt.size(20, 20)

    color:
    {
        const baseColor = checkState === Qt.Unchecked
            ? ColorTheme.colors.light10
            : ColorTheme.colors.light4

        return hovered && enabled
            ? ColorTheme.lighter(baseColor, 2)
            : baseColor
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
