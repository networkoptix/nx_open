import QtQuick 2.6

import Nx 1.0

import "private" as Private

Private.GenericValueControl
{
    id: control

    property alias zoomInPressed: control.upPressed
    property alias zoomOutPressed: control.downPressed

    property int selectedMarkerIndex: 2

    actionButtonHeight: control.supporsValuesMarker ? 44 : 56

    upButtonDecoration: Image
    {
        anchors.centerIn: parent
        source: lp("/images/ptz/ptz_zoom_plus.png")
        opacity: enabled ? 1.0 : 0.2
    }

    downButtonDecoration: Image
    {
        anchors.centerIn: parent
        source: lp("/images/ptz/ptz_zoom_minus.png")
        opacity: enabled ? 1.0 : 0.2
    }
}
