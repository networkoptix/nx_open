// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls.impl 2.15

import Nx 1.0

Item
{
    id: item

    property alias icon: icon.source
    property alias color: icon.color
    property alias enabled: mouseArea.enabled

    readonly property alias hovered: mouseArea.containsMouse

    signal clicked()

    MouseArea
    {
        id: mouseArea

        anchors.fill: item

        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onClicked:
            item.clicked()

        IconImage
        {
            id: icon
            anchors.centerIn: mouseArea
            sourceSize: Qt.size(32, 32)
        }
    }
}
