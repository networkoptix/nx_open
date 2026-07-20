// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import "private"

BaseOption
{
    id: control

    property int checkState: Qt.Unchecked

    customArea: Switch
    {
        // Self-toggle would sever the checkState binding; the owner changes state via clicked().
        toggleOnClick: false
        checkState: control.checkState
        onClicked: control.clicked()
    }
}
