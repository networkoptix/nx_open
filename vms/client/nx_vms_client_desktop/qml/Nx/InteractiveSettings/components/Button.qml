// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import Nx.Controls 1.0
import nx.vms.client.desktop 1.0

Button
{
    id: control

    property string name: ""
    property alias caption: control.text
    property string description: ""
    property bool isActive: true
    property var parametersModel: null

    GlobalToolTip.text: description

    signal activated()

    onClicked: activated()
}
