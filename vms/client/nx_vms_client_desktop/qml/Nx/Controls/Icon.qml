// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

ColoredImage
{
    id: item

    // Icon mode and state.
    property bool hovered: false
    property bool pressed: false
    property bool checked: false
    property bool selected: false

    /**
     * Icon category to obtain color customization for.
     * Default implementation attempts to deduce it from the icon path.
     */
    property string category:
    {
        let parts = item.sourcePath.split("/").filter(item => item.length)
        if (parts.length < 2)
            return ""

        parts.pop() //< Remove filename part.

        if (parts[0].endsWith(":")) //< Remove URL scheme, if present.
        {
            parts.shift()
            if (!parts.length)
                return ""
        }

        return parts.length > 1 && parts[0] == "skin"
            ? parts[1]
            : parts[0]
    }

    customColors: ColorTheme.iconImageCustomization(
        category, hovered, pressed, selected, !enabled, checked)
}
