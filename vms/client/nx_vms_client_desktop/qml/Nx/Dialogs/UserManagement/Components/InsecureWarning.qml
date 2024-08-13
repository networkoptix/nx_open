// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

RowLayout
{
    ColoredImage
    {
        Layout.alignment: Qt.AlignTop
        sourcePath: "image://skin/20x20/Solid/alert2.svg"
        primaryColor: "yellow"
        sourceSize: Qt.size(20, 20)
    }

    Text
    {
        Layout.fillWidth: true
        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.yellow_d2
        wrapMode: Text.WordWrap
        text: qsTr("Account security is important. Do not enable this setting"
            + " unless you have good reasons to do so.")
    }
}
