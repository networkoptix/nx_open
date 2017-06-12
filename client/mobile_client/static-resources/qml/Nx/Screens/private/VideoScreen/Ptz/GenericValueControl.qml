import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0

Rectangle
{
    id: control

    property bool showCentralArea: false
    property Item centralArea: null

    property alias upButton: upButtonControl
    property alias downButton: downButtonControl

    color: ColorTheme.transparent(ColorTheme.base8, 0.8)

    implicitWidth: 56
    implicitHeight: showCentralArea ? 136 : 112

    radius: 28

    IconButton
    {
        id: upButtonControl

        width: control.width
        height: width
        padding: 0
    }

    IconButton
    {
        id: downButtonControl

        width: control.width
        height: width
        anchors.bottom: parent.bottom
        padding: 0
    }

    onCentralAreaChanged:
    {
        centralArea.parent = control
        centralArea.anchors.centerIn = control
        centralArea.visible = Qt.binding(function() { return control.showCentralArea })
    }
}

