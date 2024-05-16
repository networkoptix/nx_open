// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx.Core 1.0
import Nx.Controls 1.0

Rectangle
{
    Column
    {
        anchors.centerIn: parent
        spacing: 36

        NxCirclesPreloader
        {
            running: parent.visible
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Label
        {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: 4

            color: ColorTheme.mid
            font.pixelSize: 36
            font.weight: Font.Light

            text: qsTr("Loading...")
        }
    }

    MouseArea
    {
        anchors.fill: parent

        enabled: parent.visible
        hoverEnabled: enabled
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onWheel: wheel.accepted = true
        onClicked: mouse.accepted = true
    }
}
