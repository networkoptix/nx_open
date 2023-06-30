// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Core
import Nx.Controls
import Nx.Dialogs

Column
{
    spacing: 16

    property alias button: button

    Image
    {
        anchors.horizontalCenter: parent.horizontalCenter
        source: "image://svg/skin/placeholders/list_placeholder.svg"
        sourceSize: Qt.size(64, 64)
    }

    Text
    {
        anchors.horizontalCenter: parent.horizontalCenter
        font {pixelSize: 16; weight: Font.Normal}
        color: ColorTheme.colors.dark17
        text: qsTr("No Lists")
    }

    Text
    {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 250
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        font {pixelSize: 14; weight: Font.Normal}
        color: ColorTheme.colors.dark17
        text: qsTr("You have not created any Lists yet. With lists you can store multiple "
            + "values to use them in Event Rules. Create a new List to start adding entries.")
    }

    Button
    {
        id: button
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Create New...")
    }
}
