import QtQuick 2.6

import Nx 1.0
import Nx.Controls 1.0

GenericValueControl
{
    id: control

    property alias supportsAutoFocus: control.showCentralArea
    property bool focusInPressed: control.upButton.pressed
    property bool focusOutPressed: control.downButton.pressed

    centralAreaText: "FOCUS"

    signal autoFocusClicked()

    upButton.icon: lp("/images/ptz/ptz_plus.png")
    downButton.icon: lp("/images/ptz/ptz_minus.png")

    centralArea: IconButton
    {
        width: control.width
        height: width
        padding: 0
        icon: lp("/images/ptz/af_off.png")

        onClicked: { control.autoFocusClicked() }
    }
}
