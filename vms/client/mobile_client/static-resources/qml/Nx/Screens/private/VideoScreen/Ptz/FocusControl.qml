// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Mobile.Controls

GenericValueControl
{
    id: control

    property alias supportsFocusChanging: control.enableValueControls
    property alias supportsAutoFocus: control.showCentralArea
    readonly property bool focusInPressed: control.upButton.down
    readonly property bool focusOutPressed: control.downButton.down

    signal autoFocusClicked()

    upButton.icon.source: "image://skin/24x24/Outline/plus.svg"
    downButton.icon.source: "image://skin/24x24/Outline/minus.svg"

    centralArea: Button
    {
        text: qsTr("AF") //< Autofocus.
        type: Button.Type.LightInterface
        radius: 0
        leftPadding: 0
        rightPadding: 0
        font.pixelSize: 16
        onClicked: control.autoFocusClicked()
    }
}
