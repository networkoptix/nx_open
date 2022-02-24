// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

pragma Singleton

import QtQuick 2.11
import Nx 1.0

// We use inclusion instead of inheritance because of colors map copy in case of reading
// ColorThemeBase.colors property. Unfortunately, we cannot forbid it by const/readonly usage.

Item
{
    readonly property color window: colors.dark3
    readonly property color windowText: colors.light16
    readonly property color shadow: colors.dark5
    readonly property color text: colors.light4
    readonly property color light: colors.light10
    readonly property color buttonText: colors.light4
    readonly property color brightText: colors.light1
    readonly property color mid: colors.dark10
    readonly property color dark: colors.dark9
    readonly property color midlight: colors.dark13
    readonly property color button: colors.dark11
    readonly property color highlight: colors.brand_core
    readonly property color highlightContrast: colors.brand_contrast

    // Copy colors only once.
    readonly property var colors: colorThemeBase.colors

    function transparent(color, alpha)
    {
        return colorThemeBase.transparent(color, alpha)
    }

    function lighter(color, offset)
    {
        return colorThemeBase.lighter(color, offset)
    }

    function darker(color, offset)
    {
        return colorThemeBase.darker(color, offset)
    }

    ColorThemeBase
    {
        id: colorThemeBase
    }
}
