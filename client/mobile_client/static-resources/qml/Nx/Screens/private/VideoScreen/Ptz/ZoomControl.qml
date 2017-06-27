import QtQuick 2.6

import Nx 1.0

GenericValueControl
{
    id: control

    property bool zoomInPressed: upButton.pressed
    property bool zoomOutPressed: downButton.pressed

    centralAreaText: "ZOOM"
    upButton.icon: lp("/images/ptz/ptz_zoom_plus.png")
    downButton.icon: lp("/images/ptz/ptz_zoom_minus.png")
}
