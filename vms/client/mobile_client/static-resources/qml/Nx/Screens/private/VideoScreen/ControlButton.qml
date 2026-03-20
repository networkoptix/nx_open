// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Mobile.Controls

Button
{
    id: button

    property bool suppressDisabledState: false

    readonly property string effectiveState: button.suppressDisabledState && !button.enabled
        ? (button.checked ? "default_checked" : "default_unchecked")
        : button.state

    backgroundColor: button.parameters.colors[button.effectiveState]
    foregroundColor: button.parameters.textColors[button.effectiveState]

    leftPadding: 10
    rightPadding: 10
    icon.width: 24
    icon.height: 24
}
