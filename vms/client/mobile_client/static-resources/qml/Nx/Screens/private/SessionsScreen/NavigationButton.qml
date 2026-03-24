// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core

AbstractButton
{
    id: control

    implicitHeight: 40
    focusPolicy: Qt.StrongFocus

    background: Rectangle
    {
        color: control.checked
            ? ColorTheme.colors.dark10
            : control.hovered ? ColorTheme.colors.dark8 : "transparent"
        radius: 8
    }

    contentItem: Text
    {
        text: control.text
        color: control.checked ? ColorTheme.colors.light4 : ColorTheme.colors.light10
        font.pixelSize: 14
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        leftPadding: 16
        rightPadding: 16
        topPadding: 9.5
        bottomPadding: 9.5
    }
}
