// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml

import Nx.Controls
import Nx.Core

import nx.vms.client.desktop

Text
{
    elide: Text.ElideRight

    font.pixelSize: 14

    text: model.display

    color: model.foregroundColor ?? ColorTheme.colors.light4

    GlobalToolTip.text: model.toolTip || ""
}
