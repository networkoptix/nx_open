import QtQuick 2.6

import "private" as Private

Private.GenericValueControl
{
    id: control

    property alias supporsValuesMarker: control.showCenterArea
    property alias zoomInPressed: control.upPressed
    property alias zoomOutPressed: control.downPressed

    property int selectedMarkerIndex: 0

    actionButtonHeight: control.supporsValuesMarker ? 44 : 56

    upButtonDecoration: Text
    {
        anchors.centerIn: parent
        color: "white"
        text: "Z+"
    }

    downButtonDecoration: Text
    {
        anchors.centerIn: parent
        color: "white"
        text: "Z-"
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
                    color: current ? "white" : "lightgrey"
                    width: current ? 8 : 4
                    height: width
                    radius: width / 2
                }
            }
        }
    }
}
