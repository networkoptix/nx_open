import QtQuick 2.6

import Nx 1.0

import "private" as Private

Private.GenericValueControl
{
    id: control

    property alias supporsValuesMarker: control.showCenterArea
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

    centralAreaDelegate: Column
    {
        spacing: 4

        topPadding: 6
        bottomPadding: 6
        Repeater
        {
            model: 5

            delegate: Item
            {
                property bool current: control.selectedMarkerIndex == index

                anchors.horizontalCenter: parent.horizontalCenter

                width: 4
                height: 4

                Rectangle
                {
                    x: current ? -parent.width / 2 : 0
                    y: current ? -parent.height / 2 : 0
                    color: current ? ColorTheme.contrast1 : ColorTheme.contrast16
                    width: current ? 8 : 4
                    height: width
                    radius: width / 2
                }
            }
        }
    }
}
