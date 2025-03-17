// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import "private"

/**
 * Interactive Settings type.
 */
Placeholder
{
    property string icon: "default"
    imageSource: `image://skin/64x64/Outline/${icon}.svg#ignoreErrors`
    property bool fillWidth: true
    property bool stretchable: true
}
