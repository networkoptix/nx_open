// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

Control
{
    id: control

    contentItem: ColumnLayout
    {
        spacing: 16

        Item { Layout.fillHeight: true }

        ColoredImage
        {
            id: icon

            sourcePath: "image://skin/48x48/Outline/re_centre.svg"
            primaryColor: ColorTheme.colors.light10
            sourceSize: Qt.size(48, 48)

            Layout.alignment: Qt.AlignCenter
        }

        Text
        {
            id: title

            text: qsTr("Tap anywhere on video to center view there")
            color: ColorTheme.colors.light12
            font.pixelSize: 16
            wrapMode: Text.Wrap
            horizontalAlignment: Qt.AlignHCenter

            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
    }
}
