// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0

GenericValueControl
{
    id: control

    property alias supportsFocusChanging: control.enableValueControls
    property alias supportsAutoFocus: control.showCentralArea
    readonly property bool focusInPressed: control.upButton.down
    readonly property bool focusOutPressed: control.downButton.down

    centralAreaText: "FOCUS"

    signal autoFocusClicked()

    upButton.icon.source: lp("/images/ptz/ptz_plus.png")
    downButton.icon.source: lp("/images/ptz/ptz_minus.png")

    centralArea: IconButton
    {
        width: control.width
        height: width
        padding: 0
        icon.source: lp("/images/ptz/af_off.png")

        onClicked: { control.autoFocusClicked() }
    }
}
