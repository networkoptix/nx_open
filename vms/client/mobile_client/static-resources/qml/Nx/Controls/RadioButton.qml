// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

RadioButton
{
    id: control

    implicitWidth: indicator.implicitWidth + contentItem.implicitWidth + d.spacing

    indicator: Rectangle
    {
        implicitWidth: 20
        implicitHeight: 20
        color: control.checked ? "red" : "white"

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
    }

    contentItem: Text
    {
        text: control.text
        font.pixelSize: 16
        color: ColorTheme.windowText
    }

    NxObject
    {
        id: d

        readonly property int spacing: 10
    }
}
