// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls

import Nx.Core 1.0

DayOfWeekRow
{
    id: control

    implicitHeight: 24
    implicitWidth: 48 * 7
    spacing: 0

    delegate: Text
    {
        text: model.shortName
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: locale.weekDays.indexOf(model.day) == -1 ? ColorTheme.colors.red_d1 : ColorTheme.colors.light16
    }
}
