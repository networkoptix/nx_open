// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQml

QtObject
{
    enum Value
    {
        // The navigation stays at the bottom, but there is no vertical room to stack the secondary
        // content: sheets open from the right edge and the video timeline sits beside the video
        // rather than under it.
        HorizontalCompact,

        // A single content pane, navigation bar at the bottom.
        Compact,

        // Side panels around a compact-sized content pane.
        Medium,

        // Side panels around a full-sized content pane, if enough space for it.
        Expanded
    }
}
