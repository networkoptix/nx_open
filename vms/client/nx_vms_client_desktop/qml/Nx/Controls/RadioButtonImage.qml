// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

Image
{
    property bool checked: false
    property bool hovered: false
    property bool enabled: true

    opacity: enabled ? 1.0 : 0.3
    baselineOffset: 13
    sourceSize: Qt.size(16, 16)

    source:
    {
        var source = "qrc:///skin/theme/radiobutton"
        if (checked)
            source += "_checked"
        if (hovered)
            source += "_hover"
        return source + ".png"
    }
}
