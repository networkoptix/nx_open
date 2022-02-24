// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx 1.0
import Nx.Controls 1.0
import nx.client.desktop 1.0

Item
{
    id: control

    property alias text: label.text

    property int leftPadding: 12
    property int rightPadding: 12

    implicitWidth: leftPadding + label.width + rightPadding
    implicitHeight: 32

    Rectangle
    {
        anchors.fill: parent

        radius: 2
        color: ColorTheme.colors.red_core
        opacity: 0.2
    }

    Label
    {
        id: label

        x: control.leftPadding
        height: control.height

        verticalAlignment: Label.AlignVCenter
        font.pixelSize: 13
        color: ColorTheme.lighter(ColorTheme.colors.red_core, 3)
    }
}
