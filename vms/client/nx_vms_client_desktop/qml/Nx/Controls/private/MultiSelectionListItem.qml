// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls

CheckListItem
{
    id: control

    property bool current: false

    primaryColors.normal: ColorTheme.colors.light4
    checkedPrimaryColors.normal: "white"

    implicitHeight: 28
    leftPadding: iconSource ? 6 : 8
    rightPadding: 8
    spacing: 0

    background: Rectangle
    {
        color: control.hovered || control.current
            ? ColorTheme.colors.dark15
            : ColorTheme.colors.dark13
    }
}
