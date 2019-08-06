import QtQuick 2.6

import Nx 1.0

GenericValueControl
{
    id: control

    readonly property bool zoomInPressed: upButton.down
    readonly property bool zoomOutPressed: downButton.down

    centralAreaText: "ZOOM"
    upButton.icon.source: lp("/images/ptz/ptz_zoom_plus.png")
    downButton.icon.source: lp("/images/ptz/ptz_zoom_minus.png")
}
