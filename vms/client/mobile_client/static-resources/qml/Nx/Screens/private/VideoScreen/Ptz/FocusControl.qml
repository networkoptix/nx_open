// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Core
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
        type: Button.Type.LightInterface
        foregroundColor: ColorTheme.colors.light10
        background.opacity: 0.6
        radius: 0
        leftPadding: 0
        rightPadding: 0
        icon.source: "image://skin/24x24/Outline/af.svg"
        icon.width: 24
        icon.height: 24
        onClicked: control.autoFocusClicked()
    }
}
