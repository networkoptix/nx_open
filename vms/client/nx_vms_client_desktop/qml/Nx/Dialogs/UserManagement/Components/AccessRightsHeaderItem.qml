// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls

Item
{
    id: item

    property alias icon: icon.source
    property alias color: icon.color

    property alias enabled: mouseArea.enabled

    property bool interactive: false

    readonly property alias hovered: mouseArea.hovered

    signal clicked()

    MouseArea
    {
        id: mouseArea

        anchors.fill: item

        hoverEnabled: item.interactive
        acceptedButtons: Qt.LeftButton

        property bool hovered: false

        onClicked:
        {
            item.clicked()
            hovered = false
        }

        // Clicks temporarily disable hover.
        // This binding will restore it when the mouse re-enters the area.
        Binding on hovered
        {
            value: item.interactive && mouseArea.containsMouse
        }

        Rectangle
        {
            id: background

            x: -1
            y: -1
            width: mouseArea.width + 2
            height: mouseArea.height + 2

            color: mouseArea.hovered
                ? ColorTheme.colors.dark12
                : (item.interactive ? ColorTheme.colors.dark10 : ColorTheme.colors.dark7)

            border.color: ColorTheme.colors.dark7
        }

        IconImage
        {
            id: icon
            anchors.centerIn: mouseArea
            sourceSize: Qt.size(32, 32)
        }
    }
}
