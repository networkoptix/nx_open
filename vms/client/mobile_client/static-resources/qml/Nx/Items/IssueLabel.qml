// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Rectangle
{
    property alias text: label.text
    property alias textColor: label.color

    color: ColorTheme.colors.red_core
    radius: 4

    implicitHeight: label.implicitHeight + 4
    implicitWidth: label.implicitWidth + 10

    baselineOffset: label.y + label.baselineOffset

    Text
    {
        id: label
        anchors.centerIn: parent
        color: ColorTheme.colors.dark6
        font.pixelSize: 12
        font.weight: Font.Medium
    }
}
