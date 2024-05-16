// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Core 1.0

MenuSeparator
{
    id: separator

    leftPadding: 12
    rightPadding: 12
    topPadding: 3
    bottomPadding: 4

    width: parent ? parent.width : implicitWidth

    background: null

    contentItem: Rectangle
    {
        id: line

        implicitHeight: 1
        color: ColorTheme.colors.dark11
    }
}
