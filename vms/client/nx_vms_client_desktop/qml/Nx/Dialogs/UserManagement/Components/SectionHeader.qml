// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

Item
{
    id: control
    height: 44
    width: parent ? parent.width : headerText.width

    property alias text: headerText.text
    property int counter: 0
    property alias textLeftMargin: headerText.x

    property bool enabled: true

    baselineOffset: headerText.y + headerText.baselineOffset

    Text
    {
        id: headerText

        height: 20
        font: Qt.font({pixelSize: 16, weight: Font.Medium})
        color: ColorTheme.colors.light4
        opacity: control.enabled ? 1.0 : 0.3

        anchors.top: parent.top
        anchors.topMargin: 16
    }

    Rectangle
    {
        height: 1
        width: parent.width

        color: ColorTheme.colors.dark12
        opacity: control.enabled ? 1.0 : 0.3

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
    }
}
