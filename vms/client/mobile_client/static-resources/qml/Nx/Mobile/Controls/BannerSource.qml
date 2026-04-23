// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

Item
{
    id: control

    property int type
    property string text
    property bool closeable: type === Banner.Info || type === Banner.Success
    property var durationMs: (closeable && !action) ? 4000 : null
    property Action action: null
    property bool active: false

    property Item target: mainWindow.banner

    parent: target
    visible: false

    function trigger()
    {
        active = true
    }

    function reset()
    {
        active = false
    }
}
