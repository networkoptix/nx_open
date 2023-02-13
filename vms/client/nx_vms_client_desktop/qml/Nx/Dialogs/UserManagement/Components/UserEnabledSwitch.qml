// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx 1.0
import Nx.Controls 1.0

Switch
{
    id: control

    text: control.checked ? qsTr("Enabled user") : qsTr("Disabled user")

    color: control.checked
        ? ColorTheme.colors.green_core
        : ColorTheme.colors.light16
}
