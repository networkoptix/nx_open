// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import Nx.Core 1.0

RowLayout
{
    Image
    {
        Layout.alignment: Qt.AlignTop
        source: "image://svg/skin/user_settings/warning_icon.svg"
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
