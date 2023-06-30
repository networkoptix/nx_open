// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0
import Nx.Controls 1.0

Item
{
    id: preloader

    implicitHeight: 24
    visible: false

    NxDotPreloader
    {
        id: dots

        anchors.centerIn: preloader
        color: ColorTheme.colors.light16
    }
}
