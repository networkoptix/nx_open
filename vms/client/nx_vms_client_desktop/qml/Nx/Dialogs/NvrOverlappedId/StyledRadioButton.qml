// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Qt5Compat.GraphicalEffects

import Nx.Core 1.0
import Nx.Controls 1.0

RadioButton
{
    id: control

    property string iconSource: ""

    height: 24

    indicator: Item {}

    contentItem: Item
    {
        anchors.fill: parent

        Rectangle
        {
            anchors.fill: parent
            color: control.checked ? ColorTheme.highlight : "transparent"
            opacity: control.checked ? 0.4 : 1
        }

        Text
        {
            x: 8
            anchors.verticalCenter: parent.verticalCenter

            verticalAlignment: Text.AlignVCenter
            elide: Qt.ElideRight
            font: control.font
            text: control.text
            opacity: enabled ? 1.0 : 0.3

            color: control.currentColor
        }
    }
}
