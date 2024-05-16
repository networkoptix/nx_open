// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx.Core 1.0

Rectangle
{
    property alias text: label.text
    property alias textColor: label.color

    width: label.width + 8
    height: 16

    radius: 2

    Label
    {
        id: label

        anchors.centerIn: parent

        font.bold: true
        font.pixelSize: 10
        font.capitalization: Font.AllUppercase

        color: ColorTheme.colors.dark4
    }
}
