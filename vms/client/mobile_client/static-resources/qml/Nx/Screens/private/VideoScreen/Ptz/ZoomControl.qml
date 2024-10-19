// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0

GenericValueControl
{
    id: control

    readonly property bool zoomInPressed: upButton.down
    readonly property bool zoomOutPressed: downButton.down

    centralAreaText: "ZOOM"
    upButton.icon.source: lp("/images/ptz/ptz_zoom_plus.png")
    downButton.icon.source: lp("/images/ptz/ptz_zoom_minus.png")
}
