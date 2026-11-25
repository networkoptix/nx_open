// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

MenuSeparator
{
    id: control

    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Rectangle
    {
        implicitWidth: 100
        implicitHeight: 1
        color: ColorTheme.colors.dark17
    }
}
