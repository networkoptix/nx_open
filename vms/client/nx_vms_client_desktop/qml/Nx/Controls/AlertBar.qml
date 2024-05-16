// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: control

    property alias label: labelControl
    property alias background: backgroundControl
    property int horizontalPadding: 16

    property bool info: false

    implicitHeight: labelControl.implicitHeight + 16
    implicitWidth: parent.width

    Rectangle
    {
        id: backgroundControl

        anchors.fill: parent
        color: control.info
            ? ColorTheme.colors.dialog.infoBar
            : ColorTheme.colors.dialog.alertBar
    }

    Text
    {
        id: labelControl

        x: control.horizontalPadding
        width: control.width - control.horizontalPadding * 2
        anchors.verticalCenter: parent.verticalCenter

        wrapMode: Text.WordWrap
        color: ColorTheme.text
        font.pixelSize: FontConfig.normal.pixelSize
    }
}
