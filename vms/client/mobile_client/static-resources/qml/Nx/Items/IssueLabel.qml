// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Rectangle
{
    property alias text: label.text
    property alias textColor: label.color

    color: ColorTheme.colors.red_core
    radius: 2

    implicitHeight: 18
    implicitWidth: label.implicitWidth + 16

    Text
    {
        id: label
        anchors.centerIn: parent
        color: ColorTheme.colors.dark4
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
